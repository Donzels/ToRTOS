/**
 * @file ToRTOS.h
 * @brief Core public kernel API declarations.
 * @version 1.0.0
 * @date 2026-01-19
 * @author
 *   Donzel
 * @copyright
 *   (c) 2025 StaRT Project
 */

#ifndef __TORTOS_H_
#define __TORTOS_H_

#include "tdef.h"
#include <stddef.h>

/** Pointer to currently running thread (NULL before scheduler start). */
extern t_thread_t *t_current_thread;
extern t_uint8_t t_cur_num_of_ready_tasks;
;

/**
 * @brief Perform a normal context switch (assembly implementation).
 * @param prev Address of previous thread PSP storage location.
 * @param next Address of next thread PSP storage location.
 */
void t_normal_switch_task(t_uint32_t prev, t_uint32_t next);

/**
 * @brief First context switch to start scheduling (assembly implementation).
 * @param next Address of next thread PSP storage location.
 * @note Spelling kept for backward compatibility (frist->first).
 */
void t_first_switch_task(t_uint32_t next);

/**
 * @brief Disable interrupts.
 * @return Previous PRIMASK state (pass to t_irq_enable()).
 */
t_uint32_t t_irq_disable(void);

/**
 * @brief Restore interrupts to previous state.
 * @param disirq Saved PRIMASK returned by t_irq_disable().
 */
void t_irq_enable(t_uint32_t disirq);

/**
 * @brief Initialize a thread stack frame (Cortex-M PSP layout).
 * @param stackaddr Top address of stack (end of buffer).
 * @param entry Thread entry function.
 * @param arg Argument of entry.
 * @return PSP pointer after context frame prepared.
 */
t_uint8_t *t_stack_init(t_uint8_t *stackaddr, t_thread_entry_t entry, void *arg);

/* Doubly linked intrusive list primitives */
void t_list_init(t_list_t *l);
void t_list_insert_after(t_list_t *l, t_list_t *n);
void t_list_insert_before(t_list_t *l, t_list_t *n);
void t_list_delete(t_list_t *d);
int t_list_isempty(t_list_t *l);
unsigned int t_list_length(t_list_t* l);

/**
 * @brief Initialize core subsystems (scheduler, timer, idle thread, banner).
 * @return T_OK on success.
 */
t_status_t t_tortos_init(void);

/**
 * @brief Weak hook to print startup banner (can be overridden).
 */
void t_start_banner(void);

/**
 * @brief Formatted output (minimal printf subset).
 * @param fmt Format string.
 */
void t_printf(const char *fmt, ...);

/* Scheduler control APIs */
void t_sched_init(void);
void t_sched_start(void);
void t_sched_suspend(void);
void t_sched_resume(void);
void t_sched_switch(void);
void t_sched_remove_thread(t_thread_t *thread);
void t_sched_insert_thread(t_thread_t *thread);
void t_thread_rotate_same_prio(void);
void t_cleanup_waiting_termination_threads(void);

/**
 * @brief Put current thread to sleep (block) for tick count.
 * @param tick Number of ticks to sleep.
 */
void t_thread_sleep(t_uint32_t tick);

/**
 * @brief Terminate current thread (deferred cleanup by idle).
 */
void t_thread_exit(void);

#if (TO_USING_DYNAMIC_ALLOCATION)
void *t_malloc(size_t wanted_size);
void t_free(void *ptr);
size_t t_get_free_mem_size(void);
#endif

#if (TO_USING_STATIC_ALLOCATION)
/**
 * @brief Initialize a thread object (not yet ready).

 * @param entry Entry function pointer.
 * @param stackaddr Stack memory base address.
 * @param stacksize Stack size in bytes.
 * @param priority Thread priority.
 *                - When TO_LOWER_PRIORITY_NUM_HIGHER_PRIORITY is 0: (0 = lowest).
 *                - When TO_LOWER_PRIORITY_NUM_HIGHER_PRIORITY is 1: (0 = highest).
 * @param time_slice Time slice (ticks).
 * @param arg Argument of entey.
 * @param thread Thread control block.
 * @return T_OK on success, else error code.
 */
t_status_t t_thread_create_static(t_thread_entry_t entry, void *stackaddr, t_uint32_t stacksize, t_int8_t priority, void *arg, t_uint32_t time_slice, t_thread_t *thread);
/**
 * @brief Move an initialized thread into ready state.
 * @param thread Thread object.
 * @return T_OK on success.
 */
#endif /* TO_USING_DYNAMIC_ALLOCATION */
#if (TO_USING_DYNAMIC_ALLOCATION)
t_status_t t_thread_create(t_thread_entry_t entry, t_uint32_t stacksize, t_int8_t priority, void *arg, t_uint32_t time_slice, t_thread_t **thread_handle);
#endif /* TO_USING_DYNAMIC_ALLOCATION */

t_status_t t_thread_startup(t_thread_t *thread);

/* Thread lifecycle / control */
t_status_t t_thread_delete(t_thread_t *thread);
t_status_t t_thread_suspend(t_thread_t *thread);
t_status_t t_thread_ctrl(t_thread_t *thread, t_uint32_t cmd, void *arg);
t_status_t t_thread_restart(t_thread_t *thread);

/* Timer subsystem */
void t_timer_list_init(void);
void t_mdelay(t_uint32_t ms);
void t_delay(t_uint32_t tick);
t_status_t t_timer_init(t_timer_t *timer, void (*timeout_func)(void *p), void *p, t_uint32_t tick);
void t_timer_check(void);
void t_tick_increase(void);
t_uint32_t t_tick_get(void);
t_uint32_t get_tick_diff(t_uint32_t start_tick, t_uint32_t end_tick);
t_status_t t_timer_ctrl(t_timer_t *timer, t_uint32_t cmd, void *arg);
t_status_t t_timer_stop(t_timer_t *timer);
t_status_t t_timer_start(t_timer_t *timer);
void timeout_function(void *p);

#if TO_USING_IPC
/* IPC: semaphore / mutex / message queue APIs */
t_status_t t_ipc_delete(t_ipc_t *ipc);

#if TO_USING_SEMAPHORE
#if (TO_USING_STATIC_ALLOCATION)
t_status_t t_sema_create_static(t_uint16_t max_count, t_uint16_t init_count, t_uint8_t mode, t_ipc_t *ipc);
#endif /* TO_USING_STATIC_ALLOCATION */
#if (TO_USING_DYNAMIC_ALLOCATION)
t_status_t t_sema_create(t_uint16_t max_count, t_uint16_t init_count, t_uint8_t mode, t_ipc_t **ipc_handle);
#endif /* TO_USING_DYNAMIC_ALLOCATION */
t_status_t t_sema_send(t_ipc_t *ipc);
t_status_t t_sema_recv(t_ipc_t *ipc, t_int32_t timeout);

#if (TO_USING_STATIC_ALLOCATION)
#define T_SEMA_CREATE_STATIC(max_count, init_count, mode, sema) t_sema_create_static(max_count, init_count, mode, sema)
#endif /* TO_USING_STATIC_ALLOCATION */
#if (TO_USING_DYNAMIC_ALLOCATION)
#define T_SEMA_CREATE(max_count, init_count, mode, sema_handle) t_sema_create(max_count, init_count, mode, sema_handle)
#endif /* TO_USING_DYNAMIC_ALLOCATION */
#define T_SEMA_DELETE(sema)             t_ipc_delete(sema)
#define T_SEMA_ACQUIRE(sema, timeout)   t_sema_recv(sema, timeout) 
#define T_SEMA_RELEASE(sema)            t_sema_send(sema)  
#endif /* TO_USING_SEMAPHORE */

#if (TO_USING_MUTEX || TO_USING_RECURSIVE_MUTEX)
#if (TO_USING_STATIC_ALLOCATION)
t_status_t t_mutex_create_static_base(t_ipc_type_t ipc_type, t_uint8_t mode, t_ipc_t *ipc);
#endif /* TO_USING_STATIC_ALLOCATION */
#if (TO_USING_DYNAMIC_ALLOCATION)
t_status_t t_mutex_create_base(t_ipc_type_t ipc_type, t_uint8_t mode, t_ipc_t **ipc_handle);
#endif /* TO_USING_DYNAMIC_ALLOCATION */
t_status_t t_mutex_send_base(t_ipc_t *ipc);
t_status_t t_mutex_recv_base(t_ipc_t *ipc, t_int32_t timeout);
#endif
#if TO_USING_MUTEX
#if (TO_USING_STATIC_ALLOCATION)
#define T_MUTEX_CREATE_STATIC(mode, mutex)  t_mutex_create_static_base(IPC_MUTEX, mode, mutex)
#endif /* TO_USING_STATIC_ALLOCATION */
#if (TO_USING_DYNAMIC_ALLOCATION)
#define T_MUTEX_CREATE(mode, mutex_handle)  t_mutex_create_static_base(IPC_MUTEX, mode, mutex_handle)
#endif /* TO_USING_DYNAMIC_ALLOCATION */
#define T_MUTEX_DELETE(mutex)           t_ipc_delete(mutex)      
#define T_MUTEX_ACQUIRE(mutex, timeout) t_mutex_recv_base(mutex, timeout) 
#define T_MUTEX_RELEASE(mutex)          t_mutex_send_base(mutex)
#endif
#if TO_USING_RECURSIVE_MUTEX
#if (TO_USING_STATIC_ALLOCATION)
#define T_MUTEX_RECURSIVE_CREATE_STATIC(mode, mutex)    t_mutex_create_static_base(IPC_RECURSIVE_MUTEX, mode, mutex)
#endif /* TO_USING_STATIC_ALLOCATION */
#if (TO_USING_DYNAMIC_ALLOCATION)
#define T_MUTEX_RECURSIVE_CREATE(mode, mutex_handle)    t_mutex_create_base(IPC_RECURSIVE_MUTEX, mode, mutex_handle)
#endif /* TO_USING_DYNAMIC_ALLOCATION */
#define T_MUTEX_RECURSIVE_DELETE(mutex)             t_ipc_delete(mutex) 
#define T_MUTEX_RECURSIVE_ACQUIRE(mutex, timeout)   t_mutex_recv_base(mutex, timeout) 
#define T_MUTEX_RECURSIVE_RELEASE(mutex)            t_mutex_send_base(mutex)
#endif
#if TO_USING_QUEUE
#if (TO_USING_STATIC_ALLOCATION)
t_status_t t_queue_create_static(void *queue_pool, t_uint16_t queue_length, t_uint16_t item_size, t_uint8_t mode, t_ipc_t *ipc);
#endif /* TO_USING_STATIC_ALLOCATION */
#if (TO_USING_DYNAMIC_ALLOCATION)
t_status_t t_queue_create(t_uint16_t queue_length, t_uint16_t item_size, t_uint8_t mode, t_ipc_t **ipc_handle);
#endif /* TO_USING_DYNAMIC_ALLOCATION */
t_status_t t_queue_send(t_ipc_t *ipc, const void *data, t_int32_t timeout);
t_status_t t_queue_recv(t_ipc_t *ipc, void *data, t_int32_t timeout);

#if (TO_USING_STATIC_ALLOCATION)
#define T_QUEUE_CREATE_STATIC(queue_pool, queue_length, item_size, mode, queue)\
            t_queue_create_static(queue_pool, queue_length, item_size, mode, queue)
#endif /* TO_USING_STATIC_ALLOCATION */
#if (TO_USING_DYNAMIC_ALLOCATION)
#define T_QUEUE_CREATE(queue_length, item_size, mode, queue_handle)\
            t_queue_create(queue_length, item_size, mode, queue_handle)
#endif /* TO_USING_DYNAMIC_ALLOCATION */
#define T_QUEUE_DELETE(queue)               t_ipc_delete(queue) 
#define T_QUEUE_SEND(queue, data, timeout)  t_queue_send(queue, data, timeout)    
#define T_QUEUE_RECV(queue, data, timeout)  t_queue_recv(queue, data, timeout)
#endif

#endif /* TO_USING_IPC */
#if (TO_LOWER_PRIORITY_NUM_HIGHER_PRIORITY)
/**
 * @brief Find first (least significant) bit set.
 * @param value Input value.
 * @return Bit index starting at 1, or 0 if value is 0.
 */
int __t_ffs(int value);
#else
/**
 * @brief Find last (most significant) bit set.
 * @param value Input value.
 * @return Bit index starting at 1, or 0 if value is 0.
 */
int __t_fls(int value);
#endif

#endif /* __TORTOS_H_ */
