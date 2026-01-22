/**
 * @file thread.c
 * @brief Thread management: creation, lifecycle, sleep, delete, restart.
 * @version 1.0.0
 * @date 2026-01-19
 * @author
 *   Donzel
 * @note
 *   History:
 *     - 2026-01-19 1.0.0 Donzel: Translate internal comments into English.
 */

#include "ToRTOS.h"

/* External scheduler structures */
extern t_list_t t_thread_ready_lists[TO_THREAD_PRIORITY_MAX];
extern t_uint32_t t_thread_ready_priority_group;
extern t_list_t t_thread_waiting_termination_list;

/**
 * @brief Low-level field initialization (no state / ready list insertion).
 */
static void _t_thread_create(t_thread_entry_t entry, 
                           void *stackaddr,
                           t_uint32_t stacksize,
                           t_int8_t priority,
                           void *arg,
                           t_uint32_t time_slice,
                           t_thread_t *thread)                    
{
    t_list_init(&thread->tlist);

    thread->entry = entry;
    thread->arg = arg;
    thread->stackaddr = stackaddr;
    thread->stacksize = stacksize;
    thread->current_priority = priority;
    thread->init_priority = priority;
    thread->number_mask = 1UL << thread->current_priority;

    /* Prepare initial stacked context (PSP). */
    thread->psp = (void *)t_stack_init((t_uint8_t *)stackaddr + stacksize,
                                       entry,
                                       arg);

    thread->init_tick = time_slice;
    thread->remaining_tick = time_slice;
}
#if (TO_USING_STATIC_ALLOCATION)
/**
 * @brief Public initialization (allocates timer and sets INIT state).
 */
t_status_t t_thread_create_static(t_thread_entry_t entry, 
                         void *stackaddr,
                         t_uint32_t stacksize,
                         t_int8_t priority,
                         void *arg,
                         t_uint32_t time_slice,
                         t_thread_t *thread)
{
    if (!thread || !entry || !stackaddr || 0 == stacksize)
        return T_NULL;
    if (priority < 0 || priority >= TO_THREAD_PRIORITY_MAX)
        return T_INVALID;
    if (0 == time_slice)
        return T_INVALID;

    _t_thread_create(entry, stackaddr, stacksize, priority, arg, time_slice, thread);

    /* Initialize per-thread timer (sleep/timeouts). */
    if (t_timer_init(&(thread->timer), timeout_function, thread, time_slice) != T_OK)/* tick useless in here */
        return T_ERR;

#if (TO_USING_STATIC_ALLOCATION && TO_USING_DYNAMIC_ALLOCATION)
    thread->is_static_allocated = 1;
#endif

    thread->status = TO_THREAD_INIT;
    return T_OK;
}
#endif

#if (TO_USING_DYNAMIC_ALLOCATION)
t_status_t t_thread_create(t_thread_entry_t entry, t_uint32_t stacksize, t_int8_t priority, void *arg, t_uint32_t time_slice, t_thread_t **thread_handle)
{
    if (!entry || 0 == stacksize)
        return T_NULL;
    if (priority < 0 || priority >= TO_THREAD_PRIORITY_MAX)
        return T_INVALID;
    if (0 == time_slice)
        return T_INVALID;
    t_thread_t *thread = t_malloc(sizeof(t_thread_t));
    if(!thread)
        return T_ERR;

    void *stackaddr = t_malloc(stacksize);
    if(!stackaddr)
    {
        t_free(thread);
        return T_ERR;
    }
           
    _t_thread_create(entry, stackaddr, stacksize, priority, arg, time_slice, thread);

    /* Initialize per-thread timer (sleep/timeouts). */
    if (t_timer_init(&(thread->timer), timeout_function, thread, time_slice) != T_OK)/* tick useless in here */
        return T_ERR;

#if (TO_USING_STATIC_ALLOCATION && TO_USING_DYNAMIC_ALLOCATION)
    thread->is_static_allocated = 0;
#endif

    if(thread_handle)    
        *thread_handle = thread;   

    thread->status = TO_THREAD_INIT;
    return T_OK;
}
#endif /* TO_USING_DYNAMIC_ALLOCATION */

/**
 * @brief Transition from INIT to READY: insert into ready queue.
 */
t_status_t t_thread_startup(t_thread_t *thread)
{
    register t_uint32_t level;

    if (!thread)
        return T_NULL;
    if (TO_THREAD_DELETED == thread->status)
        return T_ERR;

    level = t_irq_disable();

    thread->current_priority = thread->init_priority;
    thread->status = TO_THREAD_READY;
    thread->remaining_tick = thread->init_tick;

    t_thread_ready_priority_group |= thread->number_mask;
    t_list_insert_before(&(t_thread_ready_lists[thread->current_priority]),
                         &(thread->tlist));                  

    t_irq_enable(level);
    return T_OK;
}

/**
 * @brief Mark a thread TERMINATED (deferred reclamation by idle).
 */
t_status_t t_thread_delete(t_thread_t *thread)
{
    if (!thread)
        return T_NULL;

    if (TO_THREAD_TERMINATED == thread->status)
        return T_OK;
    if (TO_THREAD_DELETED == thread->status)
        return T_ERR;

    t_sched_remove_thread(thread);
    t_timer_stop(&(thread->timer));

    thread->status = TO_THREAD_TERMINATED;
    t_list_insert_before(&t_thread_waiting_termination_list, &(thread->tlist));
    return T_OK;
}

/**
 * @brief Sleep current thread for specified ticks (blocking).
 */
void t_thread_sleep(t_uint32_t tick)
{

    t_sched_remove_thread(t_current_thread);
    t_current_thread->status = TO_THREAD_SUSPEND;

    t_timer_stop(&(t_current_thread->timer));
    t_timer_ctrl(&(t_current_thread->timer), TO_TIMER_SET_TIME, &tick);
    t_timer_start(&(t_current_thread->timer));

    t_sched_switch();
}

/**
 * @brief Alias to t_thread_sleep().
 */
void t_delay(t_uint32_t tick)
{
    t_thread_sleep(tick);
}

/**
 * @brief Explicitly suspend a thread (not using timer).
 */
t_status_t t_thread_suspend(t_thread_t *thread)
{
    register t_uint32_t level;
    if (!thread)
        return T_NULL;

    level = t_irq_disable();
    t_sched_remove_thread(thread);
    thread->status = TO_THREAD_SUSPEND;
    t_irq_enable(level);
    return T_OK;
}

/**
 * @brief Generic control/query for thread properties.
 */
t_status_t t_thread_ctrl(t_thread_t *thread, t_uint32_t cmd, void *arg)
{
    if (!thread)
        return T_NULL;

    switch (cmd)
    {
    case TO_THREAD_GET_STATUS:
        if (arg)
            *(t_int32_t *)arg = thread->status;
        return T_OK;
    case TO_THREAD_GET_PRIORITY:
        if (arg)
            *(t_uint8_t *)arg = thread->current_priority;
        return T_OK;
    case TO_THREAD_SET_PRIORITY:
        if (arg)
        {
            thread->current_priority = *(t_uint8_t *)arg;
            thread->number_mask = 1UL << thread->current_priority;
            return T_OK;
        }
        return T_ERR;
    default:
        return T_UNSUPPORTED;
    }
}

/**
 * @brief Reclaim all TERMINATED threads (move to DELETED).
 */
void t_cleanup_waiting_termination_threads(void)
{
    register t_uint32_t level = t_irq_disable();
    while (!t_list_isempty(&t_thread_waiting_termination_list))
    {
        t_thread_t *thread = T_LIST_ENTRY(t_thread_waiting_termination_list.next,
                                          t_thread_t,
                                          tlist);
        thread->status = TO_THREAD_DELETED;
        t_list_delete(&(thread->tlist));
#if ((1 == TO_USING_DYNAMIC_ALLOCATION) && (0 == TO_USING_STATIC_ALLOCATION))
        t_free(thread->stackaddr);
        t_free(thread);     
#endif
#if (TO_USING_STATIC_ALLOCATION && TO_USING_DYNAMIC_ALLOCATION)
        if(!thread->is_static_allocated)
        {
            t_free(thread->stackaddr);
            t_free(thread);     
        }   
#endif
    }
    t_irq_enable(level);
}

/**
 * @brief Restart a DELETED thread (reinitialize context & timer).
 */
t_status_t t_thread_restart(t_thread_t *thread)
{
    if (!thread)
        return T_NULL;
    if (thread->status != TO_THREAD_DELETED)
        return T_ERR;

    register t_uint32_t level = t_irq_disable();
    t_list_t *p = t_thread_waiting_termination_list.next;
    while (p != &t_thread_waiting_termination_list)
    {
        t_thread_t *t = T_LIST_ENTRY(p, t_thread_t, tlist);
        if (t == thread)
        {
            t_list_delete(&thread->tlist);
            break;
        }
        p = p->next;
    }
    t_irq_enable(level);

    _t_thread_create(
                   thread->entry,
                   thread->stackaddr,
                   thread->stacksize,
                   thread->init_priority,
                   thread->arg,
                   thread->timer.init_tick,
                   thread);

    t_timer_init(&(thread->timer), timeout_function, thread, thread->timer.init_tick);

    thread->status = TO_THREAD_READY;
    return t_thread_startup(thread);
}

/**
 * @brief Terminate the current thread and trigger reschedule (never returns).
 */
void t_thread_exit(void)
{
    if (!t_current_thread)
        return;

    register t_uint32_t level = t_irq_disable();

    t_sched_remove_thread(t_current_thread);
    t_timer_stop(&(t_current_thread->timer));

    t_current_thread->status = TO_THREAD_TERMINATED;
    t_list_insert_before(&t_thread_waiting_termination_list, &(t_current_thread->tlist));

    t_irq_enable(level);

    t_sched_switch();

    for (;;)
    {
        /** Infinite loop safeguard: execution should not reach here if switch succeeds. */
    }
}
