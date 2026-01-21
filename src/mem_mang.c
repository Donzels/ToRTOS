/**
 * @file mem_mang.c
 * @brief  
 * A sample implementation of t_malloc() and t_free() that permits
 * allocated blocks to be freed, but does not combine adjacent free blocks
 * into a single larger block (and so will fragment memory).
 * @version 1.0.0
 * @date 2026-01-19
 * @author
 *   Donzel
 */
#include "ToRTOS.h"
#include <stddef.h>

#if (TO_USING_DYNAMIC_ALLOCATION)
#define TO_BYTE_ALIGN           (8)
/* A few bytes might be lost to byte aligning the memory start address. */
#define TO_ADJUSTED_MEM_SIZE    (TO_DYNAMIC_MEM_SIZE - TO_BYTE_ALIGN)
#define TO_BYTE_ALIGN_MASK      (TO_BYTE_ALIGN - 1)

/*
 * Initialises the memory structures before their first use.
 */
static void t_mem_init(void);

static t_uint8_t t_mem[TO_DYNAMIC_MEM_SIZE];

/* Define the linked list structure.  This is used to link free blocks in order
of their size. */
typedef struct
{
    t_list_t tlist;    /*<< The next free block in the list. */
    size_t block_size; /*<< The size of the free block. */
} t_mem_link_t;

static const t_uint16_t t_struct_size = ((sizeof(t_mem_link_t) + (TO_BYTE_ALIGN_MASK)) & ~(TO_BYTE_ALIGN_MASK));
#define t_block_size_min    ((size_t)(t_struct_size * 2))

/* Create a sentinel of free list. */
static t_list_t free_list;

/* Keeps track of the number of free bytes remaining, but says nothing about
fragmentation. */
static size_t t_free_bytes_remain = TO_ADJUSTED_MEM_SIZE;

/* STATIC FUNCTIONS ARE DEFINED AS MACROS TO MINIMIZE THE FUNCTION CALL DEPTH. */

/*
 * Insert a block into the list of free blocks - which is ordered by size of
 * the block.  Small blocks at the start of the list and large blocks at the end
 * of the list.
 */
static void t_insert_block_into_freelist(t_mem_link_t *block_to_insert)
{
    t_list_t *sentinel = &free_list;
    t_list_t *p = sentinel;

    /* Iterate through the list until a block is found that has a larger size */
    /* than the block we are inserting. */
    while (p->next != sentinel)
    {
        t_mem_link_t *next_block = T_LIST_ENTRY(p->next, t_mem_link_t, tlist);
        if (next_block->block_size > block_to_insert->block_size)
            break;
        p = p->next;
    }
    /* Update the list to include the block being inserted in the correct */
    /* position. */
    t_list_insert_after(p, &block_to_insert->tlist);
}
/*-----------------------------------------------------------*/

void *t_malloc(size_t wanted_size)
{
    t_mem_link_t *block, *new_block_link;
    static int is_inited = 0;
    void *mem_return = NULL;

    t_sched_suspend();
    {
        /* If this is the first call to malloc then the memory will require
        initialisation to setup the list of free blocks. */
        if (is_inited == 0)
        {
            t_mem_init();
            is_inited = 1;
        }

        /* The wanted size is increased so it can contain a t_mem_link_t
        structure in addition to the requested amount of bytes. */
        if (wanted_size > 0)
        {
            wanted_size += t_struct_size;

            /* Ensure that blocks are always aligned to the required number of bytes. */
            if ((wanted_size & TO_BYTE_ALIGN_MASK) != 0)
            {
                /* Byte alignment required. */
                wanted_size += (TO_BYTE_ALIGN - (wanted_size & TO_BYTE_ALIGN_MASK));
            }
        }

        if ((wanted_size > 0) && (wanted_size < TO_ADJUSTED_MEM_SIZE))
        {
            /* Blocks are stored in byte order - traverse the list from the start
            (smallest) block until one of adequate size is found. */
            t_list_t *sentinel = &free_list;
            t_list_t *p = sentinel;
            while (p->next != sentinel)
            {
                t_mem_link_t *next_block = T_LIST_ENTRY(p->next, t_mem_link_t, tlist);
                if (next_block->block_size > wanted_size)
                    break;
                p = p->next;
            }

            /* If we found the end marker(sentinel) then a block of adequate size was not found. */
            if (p->next != sentinel)
            {
                /* Return the memory space - jumping over the t_mem_link_t structure
                at its start. */
                block = T_LIST_ENTRY(p->next, t_mem_link_t, tlist);
                mem_return = (void *)(((t_uint8_t *)block) + t_struct_size);

                /* This block is being returned for use so must be taken out of the
                list of free blocks. */
                t_list_delete(p->next);

                /* If the block is larger than required it can be split into two. */
                if ((block->block_size - wanted_size) > t_block_size_min)
                {
                    /* This block is to be split into two.  Create a new block
                    following the number of bytes requested. The void cast is
                    used to prevent byte alignment warnings from the compiler. */
                    new_block_link = (void *)(((t_uint8_t *)block) + wanted_size);

                    /* Calculate the sizes of two blocks split from the single
                    block. */
                    new_block_link->block_size = block->block_size - wanted_size;
                    block->block_size = wanted_size;

                    /* Insert the new block into the list of free blocks. */
                    t_insert_block_into_freelist((new_block_link));
                }

                t_free_bytes_remain -= block->block_size;
            }
        }
    }
    t_sched_resume();

    return mem_return;
}
/*-----------------------------------------------------------*/

void t_free(void *ptr)
{
    t_uint8_t *puc = (t_uint8_t *)ptr;
    t_mem_link_t *block;

    if (ptr != NULL)
    {
        /* The memory being freed will have an t_mem_link_t structure immediately
        before it. */
        puc -= t_struct_size;

        /* This unexpected casting is to keep some compilers from issuing
        byte alignment warnings. */
        block = (void *)puc;

        t_sched_suspend();
        {
            /* Add this block to the list of free blocks. */
            t_insert_block_into_freelist(block);
            t_free_bytes_remain += block->block_size;
        }
        t_sched_resume();
    }
}
/*-----------------------------------------------------------*/

size_t t_get_free_mem_size(void)
{
    return t_free_bytes_remain;
}
/*-----------------------------------------------------------*/

static void t_mem_init(void)
{
    t_mem_link_t *first_free_block;
    t_uint8_t *align_mem;

    t_list_init(&free_list);
    
    /* Ensure the memory starts on a correctly aligned boundary. */
    align_mem = (t_uint8_t *)(((size_t)t_mem + TO_BYTE_ALIGN_MASK) & (~((size_t)(TO_BYTE_ALIGN_MASK))));

    /* To start with there is a single free block that is sized to take up the
    entire memory space. */
    first_free_block = (void *)align_mem;
    first_free_block->block_size = TO_ADJUSTED_MEM_SIZE;
    t_list_insert_after(&free_list, &first_free_block->tlist);
}
/*-----------------------------------------------------------*/
#endif /* TO_USING_DYNAMIC_ALLOCATION */
