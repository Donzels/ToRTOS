/**
 * @file tdef.h
 * @brief Kernel fundamental type and structure definitions.
 * @version 1.0.0
 * @date 2026-01-19
 * @author
 *   Donzel
 */

#ifndef __TDEF_H_
#define __TDEF_H_

#include "ToRTOS_Config.h"

/* Fixed width integer aliases */
typedef signed char         t_int8_t;
typedef unsigned char       t_uint8_t;
typedef short               t_int16_t;
typedef unsigned short      t_uint16_t;
typedef int                 t_int32_t;
typedef unsigned int        t_uint32_t;
typedef long long           t_int64_t;
typedef unsigned long long  t_uint64_t;

/**
 * @brief Generic status codes used across subsystems.
 */
typedef enum
{
    T_OK = 0,          /**< Operation succeeded */
    T_ERR = -1,        /**< Generic error */
    T_TIMEOUT = -2,    /**< Timeout expired */
    T_BUSY = -3,       /**< Resource busy */
    T_INVALID = -4,    /**< Invalid argument */
    T_NULL = -5,       /**< NULL pointer */
    T_DELETED = -6,    /**< Object deleted / invalid */
    T_UNSUPPORTED = -7 /**< Unsupported operation */
} t_status_t;

typedef void (*t_thread_entry_t)(void *arg);

/**
 * @brief Intrusive doubly-linked list node.
 */
typedef struct list
{
    struct list *next;
    struct list *prev;    
} t_list_t;

/**
 * @brief Software timer control block.
 */
typedef struct timer
{
    t_list_t    row[TO_TIMER_SKIP_LIST_LEVEL];  /**< Skip-list / level nodes */
    void        (*timeout_func)(void *p);       /**< Timeout callback */
    void        *p;                             /**< User parameter */
    t_uint32_t  init_tick;                      /**< Initial duration (ticks) */
    t_uint32_t  timeout_tick;                   /**< Absolute expiration tick */
} t_timer_t;

/**
 * @brief Thread control block.
 */
typedef struct thread
{
    void        *psp;               /**< Saved process stack pointer 
                                    (hardware context next restore point) */
    t_thread_entry_t    entry;      /**< Entry function */
    void        *arg;
    void        *stackaddr;         /**< Stack base (low address) */
    t_uint32_t  stacksize;          /**< Stack size in bytes */
    t_list_t    tlist;              /**< Run / wait queue list node */
    t_uint8_t   current_priority;   /**< Current (possibly boosted) priority */
    t_uint8_t   init_priority;      /**< Original priority at creation */
    t_uint32_t  number_mask;        /**< Bit mask for ready group */

    t_uint32_t  init_tick;          /**< Time slice length (ticks) */
    t_uint32_t  remaining_tick;     /**< Remaining time slice */
    t_int32_t   status;             /**< Thread lifecycle status flags */
    t_timer_t   timer;              /**< Per-thread sleep/timeout timer */

#if (TO_USING_STATIC_ALLOCATION && TO_USING_DYNAMIC_ALLOCATION)
    t_uint8_t   is_static_allocated;
#endif
} t_thread_t;

#if TO_USING_IPC
typedef enum
{
#if TO_USING_SEMAPHORE    
    IPC_SEMA = 0,     /* Semaphore */
#endif
#if TO_USING_MUTEX
    IPC_MUTEX,        /* Mutex */
#endif
#if TO_USING_RECURSIVE_MUTEX
    IPC_RECURSIVE_MUTEX,    /* Recursive Mutex */
#endif
#if TO_USING_QUEUE
    IPC_QUEUE         /* Message Queue */
#endif
} t_ipc_type_t;

/* Queue buffer pointers */
typedef struct
{
    t_uint8_t *head;       /* Start of buffer */
    t_uint8_t *tail;       /* End marker */
    t_uint8_t *read_from;  /* Last read position */
    t_uint8_t *write_to;   /* Next write position */
} t_queue_pointers_t;

/* Mutex / Semaphore extra information */
typedef struct
{
    t_thread_t  *holder;        /* Current mutex owner */
    t_uint16_t  recursive;      /* Recursive count for mutex */
    t_uint8_t   original_prio;  /* Owner's original priority */
} t_sema_data_t;

typedef struct
{
    t_ipc_type_t   type;          /* IPC type */
    union
    {
        t_queue_pointers_t queue;     /* Used for queue */
        t_sema_data_t   sema;      /* Used for semaphore/mutex */
    } u;

    t_list_t     wait_list;      /* Thread wait list */

    t_uint16_t   msg_waiting;    /* Current item count or resource count */
    t_uint16_t   length;         /* Max number of items or max count */
    t_uint16_t   item_size;      /* Size of each item */
    t_uint8_t    status;         /* 1=valid, 0=deleted */
    t_uint8_t    mode;           /* FIFO / PRIO */
#if (TO_USING_STATIC_ALLOCATION && TO_USING_DYNAMIC_ALLOCATION)
    t_uint8_t   is_static_allocated;
#endif
} t_ipc_t;
#if (TO_USING_MUTEX || TO_USING_RECURSIVE_MUTEX)
#define DUMMY_PRIORITY  (0xFF)
#endif
#endif /* TO_USING_IPC */

#define t_inline static inline __attribute__((always_inline))

/* Thread status flags */
#define TO_THREAD_READY       0x01
#define TO_THREAD_SUSPEND     0x02
#define TO_THREAD_TERMINATED  0x08
#define TO_THREAD_RUNNING     0x10
#define TO_THREAD_DELETED     0x20
#define TO_THREAD_INIT        0x80

/* Timer control command codes */
#define TO_TIMER_GET_TIME     0x01
#define TO_TIMER_SET_TIME     0x02

/* Thread control commands */
#define TO_THREAD_GET_STATUS    0x01
#define TO_THREAD_SET_STATUS    0x02
#define TO_THREAD_GET_PRIORITY  0x03
#define TO_THREAD_SET_PRIORITY  0x04

#if TO_DEBUG
#define TO_DEBUG_INFO 0x01
#define TO_DEBUG_WARN 0x02
#define TO_DEBUG_ERR  0x03
#endif

#if TO_USING_IPC
#define TO_IPC_FLAG_FIFO  0x00 /**< FIFO ordering */
#define TO_IPC_FLAG_PRIO  0x01 /**< Priority ordering */
#define TO_WAITING_FOREVER (0xFFFFFFFFUL) /**< Block forever */
#define TO_WAITING_NO      ((t_int32_t)(0))  /**< Non-blocking */

#if TO_USING_RECURSIVE_MUTEX
#define MUTEX_RECURSIVE_COUNT_MAX 0xFF
#endif

#endif /* TO_USING_IPC */

#ifndef __weak
#define __weak  __attribute__((weak))
#endif

#if TO_DEBUG
/**
 * @brief Simple formatted debug logging macro (disabled when TO_DEBUG=0).
 */
#define T_DEBUG_LOG(level, fmt, ...)                                              \
    do {                                                                          \
        const char *slevel =                                                      \
            ((level) == TO_DEBUG_ERR)  ? "ERR" :                               \
            ((level) == TO_DEBUG_WARN) ? "WARN" : "INFO";                      \
        t_printf("[%s] " fmt, slevel, ##__VA_ARGS__);                             \
    } while (0)
#else
#define T_DEBUG_LOG(level, fmt, ...) ((void)0)
#endif /* TO_DEBUG */

#define TO_ALIGN_SIZE 4
#define T_ALIGN_UP(sz, a) ( ((sz) + ((a)-1)) & ~((a)-1) )


/**
 * @brief Obtain container struct pointer from member pointer.
 */
#define T_CONTAINER_OF(ptr, type, member) \
    ((type *)((char *)(ptr) - (unsigned long)(&((type *)0)->member)))

/**
 * @brief Get struct pointer that embeds a list node.
 */
#define T_LIST_ENTRY(node, type, member) \
    T_CONTAINER_OF(node, type, member)

#endif /* __TDEF_H_ */

