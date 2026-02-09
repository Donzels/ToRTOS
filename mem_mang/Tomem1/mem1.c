/**
 * @file mem1.c
 * @brief Advanced byte pool memory allocator for ToRTOS.
 *
 * A high-performance dynamic memory allocator with the
 * following key characteristics:
 *
 *  1. **Multi-instance support** – each @c t_byte_pool_t is self-contained;
 *     the caller can create as many pools as needed.  A default singleton
 *     pool is provided for the legacy @c t_malloc / @c t_free API.
 *
 *  2. **search_pointer (roving pointer)** – a key optimization technique.
 *     Instead of always starting from the head of the pool the next allocation
 *     search resumes from where the previous one left off. This spreads
 *     allocations across the entire pool and dramatically reduces average
 *     search time, especially under heavy churn.
 *
 *  3. **Lazy merge** – adjacent free blocks are coalesced only when they
 *     are encountered during an allocation search, NOT during @c free().
 *     This keeps the free path O(1) and amortises the merge cost.
 *
 *  4. **Address-ordered circular block list** – every block (free or
 *     allocated) is linked in memory-address order forming a circular
 *     ring that ends with a permanently-allocated sentinel.  Each block
 *     header is just two pointers:
 *       @code
 *       [next_block_ptr]   →  next block in address order (circular)
 *       [owner_ptr]        →  pool pointer (allocated) | FREE marker
 *       @endcode
 *     Block size is implicit: @c next_block – @c current_block.
 *
 *  5. **First-fit with wrap-around** – the search walks from
 *     @c search_ptr, wrapping around the ring.  The first free block
 *     that is large enough (after lazy merging) wins.
 *
 * @version 1.0.0
 * @date 2026-02-09
 * @author
 *   Donzel
 */
#include "ToRTOS.h"
#include <stddef.h>

#if (TO_USING_DYNAMIC_ALLOCATION)

/* ================================================================== */
/*                         Configuration                              */
/* ================================================================== */
#define T_BYTE_ALIGN                8u
#define T_BYTE_ALIGN_MASK           (T_BYTE_ALIGN - 1u)

/*
 * Block header layout (placed at the start of every block):
 *
 *   offset 0              : t_uint8_t *next_block   (next block, address order)
 *   offset sizeof(void *) : void      *owner        (pool ptr or FREE marker)
 *
 * Total header size = sizeof(t_byte_block_t).
 */
typedef struct t_byte_block
{
    t_uint8_t *next;
    void      *owner;
} t_byte_block_t;

#define T_BYTE_BLOCK_HEADER_SIZE    (sizeof(t_byte_block_t))

/* Compile-time check: header must be exactly two pointers. */
typedef char t_byte_block_size_check[(sizeof(t_byte_block_t) == (2u * sizeof(void *))) ? 1 : -1];

/*
 * Minimum total block size (header + payload).
 * A remainder smaller than this will NOT be split off.
 */
#define T_BYTE_BLOCK_MIN            (T_BYTE_BLOCK_HEADER_SIZE + T_BYTE_ALIGN)

/* Magic value stored in the owner field to mark a FREE block. */
#define T_BYTE_BLOCK_FREE           ((void *) 0xA5A5A5A5UL)

/* Pool identification magic ("BYTE" in ASCII). */
#define T_BYTE_POOL_MAGIC           ((t_uint32_t) 0xDEADBEEFUL)

/* ================================================================== */
/*                     Block header access macros                     */
/* ================================================================== */
/** Read / write the "next block" pointer at the beginning of a block. */
#define BLOCK_NEXT(blk)     (((t_byte_block_t *)(blk))->next)

/** Read / write the "owner" pointer right after the next-block pointer. */
#define BLOCK_OWNER(blk)    (((t_byte_block_t *)(blk))->owner)

/**
 * @brief Byte pool control block.
 *
 * Each pool manages its own contiguous memory region.
 * Multiple pools can coexist independently.
 */
typedef struct t_byte_pool
{
    t_uint8_t   *pool_start;     /**< Aligned start of pool memory */
    size_t      pool_size;      /**< Usable pool size (bytes, after alignment) */
    size_t      available;      /**< Current available bytes */
    t_uint32_t  fragments;      /**< Number of free fragments */
    t_uint8_t   *search_ptr;     /**< Roving search pointer for efficient allocation */
    t_uint8_t   *block_list;     /**< Head of circular block list */
    t_uint32_t  pool_id;        /**< Magic number for pool validation */
} t_byte_pool_t;

/* ── Byte pool multi-instance API ── */
t_status_t  t_byte_pool_create(t_byte_pool_t *pool, void *pool_start, size_t pool_size);
void        *t_byte_pool_alloc(t_byte_pool_t *pool, size_t size);
t_status_t  t_byte_pool_free(void *ptr);
size_t      t_byte_pool_available(t_byte_pool_t *pool);
t_status_t  t_byte_pool_delete(t_byte_pool_t *pool);

/* ================================================================== */
/*                    Forward declarations                            */
/* ================================================================== */
static void *_t_byte_pool_search(t_byte_pool_t *pool, size_t size);

/* ================================================================== */
/*                       Byte Pool Public API                         */
/* ================================================================== */

/**
 * @brief Create (initialise) a byte memory pool.
 *
 * Sets up the internal circular block list with one large free block and
 * an end-of-pool sentinel block that is permanently allocated.
 *
 *   @verbatim
 *   Pool memory layout after creation:
 *
 *   aligned_start                                      end_block
 *   ┌──────────┬──────────┬─── ─── ─── ───┬──────────┬──────────┐
 *   │ next_ptr │  FREE    │   free space  │ next_ptr │  owner   │
 *   │→end_block│          │               │→aligned  │ = pool   │
 *   └──────────┴──────────┴─── ─── ─── ───┴──────────┴──────────┘
 *    ← first free block (payload area) →    sentinel (always alloc)
 *   @endverbatim
 *
 * @param pool       Pointer to a caller-provided pool control block.
 * @param pool_start Start address of the raw memory region.
 * @param pool_size  Total byte count of the raw memory region.
 * @return T_OK on success, T_INVALID on bad parameters.
 */
t_status_t t_byte_pool_create(t_byte_pool_t *pool,
                              void          *pool_start,
                              size_t         pool_size)
{
    t_uint8_t *aligned_start;
    t_uint8_t *end_block;

    if (!pool || !pool_start || pool_size < (T_BYTE_BLOCK_MIN * 2u))
        return T_INVALID;

    /* Align the start address upward. */
    aligned_start = (t_uint8_t *)
        (((size_t)pool_start + T_BYTE_ALIGN_MASK) & ~((size_t)T_BYTE_ALIGN_MASK));

    /* Shrink usable size to compensate for alignment and round down. */
    pool_size -= (size_t)(aligned_start - (t_uint8_t *)pool_start);
    pool_size &= ~((size_t)T_BYTE_ALIGN_MASK);

    pool->pool_start = aligned_start;
    pool->pool_size  = pool_size;

    /* Sentinel sits at the very end of the pool. */
    end_block = aligned_start + pool_size - T_BYTE_BLOCK_HEADER_SIZE;

    /* First block: FREE – covers the whole pool except the sentinel. */
    BLOCK_NEXT(aligned_start)  = end_block;
    BLOCK_OWNER(aligned_start) = T_BYTE_BLOCK_FREE;

    /* End sentinel: always ALLOCATED (owned by pool), wraps back to start. */
    BLOCK_NEXT(end_block)  = aligned_start;
    BLOCK_OWNER(end_block) = (void *)pool;

    pool->block_list  = aligned_start;
    pool->search_ptr  = aligned_start;
    pool->available   = pool_size - (2u * T_BYTE_BLOCK_HEADER_SIZE);
    pool->fragments   = 1u;
    pool->pool_id     = T_BYTE_POOL_MAGIC;

    return T_OK;
}
/*-----------------------------------------------------------*/

/**
 * @brief Allocate memory from a byte pool.
 *
 * Uses a first-fit search starting from the roving @c search_ptr.
 * Adjacent free blocks are lazily merged during the search.
 *
 * @param pool  Pool control block.
 * @param size  Requested payload bytes (0 → returns NULL).
 * @return Pointer to usable memory, or NULL if allocation failed.
 */
void *t_byte_pool_alloc(t_byte_pool_t *pool, size_t size)
{
    void *ptr = NULL;

    if (!pool || T_BYTE_POOL_MAGIC != pool->pool_id || 0 == size)
        return NULL;

    /* Round up to alignment boundary. */
    size = (size + T_BYTE_ALIGN_MASK) & ~((size_t)T_BYTE_ALIGN_MASK);

    t_sched_suspend();
    {
        if (size <= pool->available)
        {
            ptr = _t_byte_pool_search(pool, size);
        }
    }
    t_sched_resume();

    return ptr;
}
/*-----------------------------------------------------------*/

/**
 * @brief Release previously allocated memory back to its byte pool.
 *
 * The owning pool is identified from the block header embedded just
 * before the user pointer, so the caller does not need to specify which
 * pool the block belongs to.
 *
 * Optimization: if the freed block sits before the current @c search_ptr,
 * the pointer is moved back so the next allocation will discover this
 * block sooner.
 *
 * @param ptr  Pointer previously returned by t_byte_pool_alloc / t_malloc.
 * @return T_OK on success, T_NULL / T_INVALID on error.
 */
t_status_t t_byte_pool_free(void *ptr)
{
    t_uint8_t     *block_ptr;
    t_byte_pool_t *pool;
    size_t         block_size;

    if (!ptr)
        return T_NULL;

    /* Step back over the header to reach the real block start. */
    block_ptr = (t_uint8_t *)ptr - T_BYTE_BLOCK_HEADER_SIZE;

    /* The owner field identifies – and validates – the pool. */
    pool = BLOCK_OWNER(block_ptr);
    if (!pool || T_BYTE_POOL_MAGIC != pool->pool_id)
        return T_INVALID;

    t_sched_suspend();
    {
        /* Block size = gap between this header and the next header. */
        block_size = BLOCK_NEXT(block_ptr) - block_ptr;

        /* Return capacity and count the new fragment. */
        pool->available += block_size;
        pool->fragments++;

        /* Mark block as FREE. */
        BLOCK_OWNER(block_ptr) = T_BYTE_BLOCK_FREE;

        /*
         * ── search_ptr roll-back optimization ──
         *
         * When a block located *before* the current search pointer is
         * freed, we pull search_ptr back so that the memory just
         * released is found quickly on the next allocation.
         */
        if (block_ptr < pool->search_ptr)
        {
            pool->search_ptr = block_ptr;
        }
    }
    t_sched_resume();

    return T_OK;
}
/*-----------------------------------------------------------*/

/**
 * @brief Query available bytes in a byte pool.
 *
 * @note Does not account for fragmentation – the largest single
 *       allocation may be smaller than the returned value.
 */
size_t t_byte_pool_available(t_byte_pool_t *pool)
{
    if (!pool || T_BYTE_POOL_MAGIC != pool->pool_id)
        return 0u;
    return pool->available;
}
/*-----------------------------------------------------------*/

/**
 * @brief Delete (invalidate) a byte pool.
 *
 * After deletion the pool must not be used until re-created with
 * @c t_byte_pool_create.
 */
t_status_t t_byte_pool_delete(t_byte_pool_t *pool)
{
    if (!pool)
        return T_NULL;

    t_sched_suspend();
    {
        pool->pool_id = 0u;            /* invalidate */
    }
    t_sched_resume();

    return T_OK;
}

/* ================================================================== */
/*                      Core search algorithm                         */
/* ================================================================== */

/**
 * @brief First-fit search with lazy merge.
 *
 * Starting from @c pool->search_ptr the algorithm walks the circular
 * block list.  When a free block is encountered every immediately
 * following free block is merged into it (lazy / deferred coalescing).
 * If the merged block is large enough the allocation is satisfied and
 * the remainder (if big enough) is split off as a new free block.
 *
 * After a successful allocation @c search_ptr is advanced past the
 * newly allocated block so the next search does not revisit the same
 * region – this is a key optimization for performance.
 *
 * @param pool  Pool control block (caller guarantees valid).
 * @param size  Aligned payload size in bytes.
 * @return Pointer to user payload, or NULL if no suitable block found.
 */
static void *_t_byte_pool_search(t_byte_pool_t *pool, size_t size)
{
    t_uint8_t  *current_ptr;
    t_uint8_t  *next_ptr;
    t_uint32_t  examine_blocks;
    size_t      available_bytes;

    current_ptr    = pool->search_ptr;
    examine_blocks = pool->fragments + 1u;

    /* Walk at most (fragments + 1) blocks — guarantees full wrap-around. */
    while (examine_blocks--)
    {
        /* ── Is the current block free? ── */
        if (BLOCK_OWNER(current_ptr) == T_BYTE_BLOCK_FREE)
        {
            /*
             * Lazy merge: absorb every consecutive free block that
             * immediately follows this one.  Because blocks are in
             * address order, merging is a trivial pointer update.
             */
            next_ptr = BLOCK_NEXT(current_ptr);
            while (BLOCK_OWNER(next_ptr) == T_BYTE_BLOCK_FREE)
            {
                BLOCK_NEXT(current_ptr) = BLOCK_NEXT(next_ptr);
                pool->fragments--;
                next_ptr = BLOCK_NEXT(current_ptr);
            }

            /* How many payload bytes can this (possibly merged) block hold? */
            available_bytes = next_ptr - current_ptr - T_BYTE_BLOCK_HEADER_SIZE;

            if (available_bytes >= size)
            {
                /* ── Split if the leftover is large enough ── */
                if ((available_bytes - size) >= T_BYTE_BLOCK_MIN)
                {
                    t_uint8_t *split_ptr = current_ptr
                                         + T_BYTE_BLOCK_HEADER_SIZE
                                         + size;

                    BLOCK_NEXT(split_ptr)  = BLOCK_NEXT(current_ptr);
                    BLOCK_OWNER(split_ptr) = T_BYTE_BLOCK_FREE;

                    BLOCK_NEXT(current_ptr) = split_ptr;

                    pool->fragments++;          /* new free fragment */
                }

                /* ── Mark block as ALLOCATED (owner = pool). ── */
                BLOCK_OWNER(current_ptr) = (void *)pool;

                /* Update accounting (block_size includes the header). */
                next_ptr = BLOCK_NEXT(current_ptr);
                pool->available -= (next_ptr - current_ptr);
                pool->fragments--;              /* one less free fragment */

                /*
                 * ── Advance search_ptr ──
                 *
                 * The next search starts from the block *after* the one
                 * we just allocated.  This avoids re-scanning already
                 * inspected blocks and distributes allocations evenly
                 * across the pool.
                 */
                pool->search_ptr = BLOCK_NEXT(current_ptr);

                /* Return a pointer past the header → user payload. */
                return (void *)(current_ptr + T_BYTE_BLOCK_HEADER_SIZE);
            }
        }

        /* Move to the next block in address order. */
        current_ptr = BLOCK_NEXT(current_ptr);
    }

    /* Wrapped all the way around — no suitable block found. */
    return NULL;
}

/* ================================================================== */
/*        Default (singleton) pool  +  legacy-compatible API          */
/* ================================================================== */

/** Raw backing store for the default byte pool. */
static t_uint8_t      _t_default_mem[TO_DYNAMIC_MEM_SIZE];

/** Default pool control block. */
static t_byte_pool_t  _t_default_pool;

/** One-shot flag guarding lazy initialisation. */
static t_uint8_t      _t_default_pool_inited = 0u;

/**
 * @brief Ensure the default pool has been created (lazy init, once only).
 */
static void _t_ensure_default_pool(void)
{
    if (!_t_default_pool_inited)
    {
        t_byte_pool_create(&_t_default_pool,
                           _t_default_mem,
                           TO_DYNAMIC_MEM_SIZE);
        _t_default_pool_inited = 1u;
    }
}
/*-----------------------------------------------------------*/

/**
 * @brief Allocate from the default byte pool (drop-in replacement).
 */
void *t_malloc(size_t wanted_size)
{
    _t_ensure_default_pool();
    return t_byte_pool_alloc(&_t_default_pool, wanted_size);
}
/*-----------------------------------------------------------*/

/**
 * @brief Free memory back to its owning pool (drop-in replacement).
 */
void t_free(void *ptr)
{
    t_byte_pool_free(ptr);
}
/*-----------------------------------------------------------*/

/**
 * @brief Return available bytes in the default pool.
 */
size_t t_get_free_mem_size(void)
{
    _t_ensure_default_pool();
    return t_byte_pool_available(&_t_default_pool);
}
/*-----------------------------------------------------------*/

#endif /* TO_USING_DYNAMIC_ALLOCATION */
