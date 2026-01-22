/**
 * @file scheduler.c
 * @brief Priority-based ready queue scheduler and context switching glue.
 * @version 1.0.0
 * @date 2026-01-19
 * @author
 *   Donzel
 */

#include "tdef.h"
#include "ToRTOS.h"


/** Currently running thread pointer. */
t_thread_t *t_current_thread = NULL;
t_uint8_t t_cur_num_of_ready_tasks = 0;
/** Currently running thread priority. */
t_uint8_t t_current_priority;

/** Storage for previous thread PSP pointer (used by assembly switch). */
t_uint32_t t_prev_thread_sp_p;
/** Storage for next thread PSP pointer (used by assembly switch). */
t_uint32_t t_next_thread_sp_p;
/** PendSV trigger flag (set before requesting context switch). */
t_uint32_t t_interrupt_flag;

/** Per-priority ready lists (circular list sentinel). */
t_list_t t_thread_ready_lists[TO_THREAD_PRIORITY_MAX];
/** Bitmask indicating which priorities have at least one ready thread. */
t_uint32_t t_thread_ready_priority_group = 0;
/** List of threads waiting termination (TERMINATED -> DELETED). */
t_list_t t_thread_waiting_termination_list;

static volatile t_uint32_t t_schedulue_suspend = 0;

static inline t_uint32_t get_highest_ready_priority(t_uint32_t ready_group)
{
#if (TO_LOWER_PRIORITY_NUM_HIGHER_PRIORITY)
    return __t_ffs(ready_group) - 1;
#else
    return __t_fls(ready_group) - 1;
#endif
}

/**
 * @brief Initialize scheduler internal structures.
 */
void t_sched_init(void)
{
    t_uint8_t i;
    for (i = 0; i < TO_THREAD_PRIORITY_MAX; i++)
    {
        t_list_init(&t_thread_ready_lists[i]); 
    }
    t_list_init(&t_thread_waiting_termination_list);
    t_schedulue_suspend = 0;
    t_current_thread = NULL;
}

/**
 * @brief Start scheduling: select highest ready and perform first context switch.
 * @note Assumes at least one thread is ready.
 */
void t_sched_start(void)
{
    register t_thread_t *next_thread;
    register t_uint32_t highest_ready_priority;

    highest_ready_priority = get_highest_ready_priority(t_thread_ready_priority_group);
    next_thread = T_LIST_ENTRY(
        t_thread_ready_lists[highest_ready_priority].next,
        t_thread_t,
        tlist);

    /* Mark chosen thread as running and reload its time slice. */
    t_current_thread = next_thread;
    t_current_priority = next_thread->current_priority;
    next_thread->status = TO_THREAD_RUNNING;
    next_thread->remaining_tick = next_thread->init_tick;

    t_first_switch_task((t_uint32_t)&t_current_thread->psp);
}

void t_sched_suspend(void)
{
    t_schedulue_suspend ++;
}

void t_sched_resume(void)
{
    t_schedulue_suspend --;
    if(0 == t_schedulue_suspend)
    {
        if(t_cur_num_of_ready_tasks > 0)
            t_sched_switch();    
    }
}
/**
 * @brief Attempt a context switch to higher-priority ready thread (if any).
 */
void t_sched_switch(void)
{
    register t_thread_t *next_thread;
    register t_thread_t *prev_thread;
    register t_uint32_t highest_ready_priority;

    if(0 != t_schedulue_suspend)/* schedule suspend */
        return;

    highest_ready_priority = get_highest_ready_priority(t_thread_ready_priority_group);
    if (highest_ready_priority >= TO_THREAD_PRIORITY_MAX)
        return;

    next_thread = T_LIST_ENTRY(
        t_thread_ready_lists[highest_ready_priority].next,
        t_thread_t,
        tlist);

    if (t_current_thread == next_thread || !next_thread)
        return;

    prev_thread = t_current_thread;
    t_current_thread = next_thread;

    if (prev_thread && TO_THREAD_RUNNING == prev_thread->status)
        prev_thread->status = TO_THREAD_READY;

    next_thread->status = TO_THREAD_RUNNING;
    t_current_priority = next_thread->current_priority;

    t_normal_switch_task((t_uint32_t)&prev_thread->psp,
                         (t_uint32_t)&next_thread->psp);
}

/**
 * @brief Remove a thread from ready queue (and clear ready bit if empty).
 * @param thread Thread to be removed.
 * @note Must not be NULL.
 */
void t_sched_remove_thread(t_thread_t *thread)
{
    register t_uint32_t level;
    if (!thread)
        return;

    level = t_irq_disable();

    t_list_delete(&thread->tlist);

    if (t_list_isempty(&t_thread_ready_lists[thread->current_priority]))
    {
        t_thread_ready_priority_group &= ~(thread->number_mask);
    }
    t_cur_num_of_ready_tasks --;

    t_irq_enable(level);
}

/**
 * @brief Insert a thread into ready queue and set ready bit.
 */
void t_sched_insert_thread(t_thread_t *thread)
{
    register t_uint32_t level;

    if (!thread)
        return;

    level = t_irq_disable();
    
    t_list_insert_before(&(t_thread_ready_lists[thread->current_priority]),
                         &(thread->tlist));
    t_thread_ready_priority_group |= thread->number_mask;
    t_cur_num_of_ready_tasks ++;

    t_irq_enable(level);
}

/**
 * @brief Voluntarily yield CPU within same priority group (round robin).
 */
void t_thread_rotate_same_prio(void)
{
    register t_uint32_t level;

    level = t_irq_disable();

    /* If only one thread at this priority, no rotation needed. */
    if(t_list_length(&t_thread_ready_lists[t_current_thread->current_priority]) <= 1U)
    {
        t_irq_enable(level);
        return;
    }

    /* Move current thread to queue tail (before head sentinel). */
    t_list_delete(&t_current_thread->tlist);
    t_list_insert_before(
        &(t_thread_ready_lists[t_current_thread->current_priority]),
        &(t_current_thread->tlist));

    t_irq_enable(level);

    t_sched_switch();
}
