/**
 * @file timer.c
 * @brief Timer management and system tick handling.
 * @version 1.0.0
 * @date 2026-01-19
 * @author
 *   Donzel
 */

#include "ToRTOS.h"
#include <stddef.h>

#define TIMER_USE_OVERFLOW_LIST 1 /*                                                                         \
                                      0: Use a single timer list; compare tick values via signed-diff.       \
                                          - Simpler code, fewer lists.                                       \
                                          - Must handle wrap-around carefully when inserting/sorting timers. \
                                      1: Use separate current/overflow timer lists for tick wrap-around.     \
                                          - Clear ordering in each list, simpler unsigned comparisons.       \
                                          - More code, but safer and deterministic on tick overflow.         \
                                  */

/** Global monotonic tick counter (wraps on overflow). */
volatile t_uint32_t s_tick;

/** Timer skip-list levels (currently level count fixed by config). */
static t_list_t s_timer_list[TO_TIMER_SKIP_LIST_LEVEL];
static t_list_t *g_cur_timer_list = &s_timer_list[0];

#if (TIMER_USE_OVERFLOW_LIST)
static t_list_t s_overflow_timer_list[TO_TIMER_SKIP_LIST_LEVEL];
static t_list_t *g_overflow_timer_list = &s_overflow_timer_list[0];
#endif

/**
 * @brief Initialize all timer list heads.
 */
void t_timer_list_init(void)
{
    int i;
    for (i = 0; i < TO_TIMER_SKIP_LIST_LEVEL; i++)
    {
        t_list_init(&s_timer_list[i]);
#if (TIMER_USE_OVERFLOW_LIST)
        t_list_init(&s_overflow_timer_list[i]);
#endif
    }
}

/**
 * @brief Convert milliseconds to system ticks.
 * @param ms Millisecond value.
 */
t_uint32_t s_tick_from_ms(t_uint32_t ms)
{
    if (ms == 0)
        return 0;
    return (ms * TO_TICK) / 1000;
}

/**
 * @brief Busy-sleep current thread for millisecond duration.
 * @param ms Milliseconds to delay.
 */
void t_mdelay(t_uint32_t ms)
{
    t_uint32_t tick = s_tick_from_ms(ms);
    t_thread_sleep(tick);
}

/**
 * @brief Get current global tick count since system start.
 */
t_uint32_t t_tick_get(void)
{
    return s_tick;
}

t_uint32_t get_tick_diff(t_uint32_t start_tick, t_uint32_t end_tick)
{
#if (TIMER_USE_OVERFLOW_LIST)
    if(end_tick >= start_tick)
        return end_tick - start_tick;
    return end_tick + 0xFFFFFFFFUL - start_tick;
#else
    return (t_int32_t)(end_tick - start_tick);
#endif
}

/**
 * @brief Initialize a software timer object.
 * @param timer Timer control block.
 * @param timeout_func Callback invoked at expiration.
 * @param p User parameter for callback.
 * @param tick Initial duration in ticks.
 */
t_status_t t_timer_init(t_timer_t *timer, void (*timeout_func)(void *p), void *p, t_uint32_t tick)
{
    if (timer == NULL || timeout_func == NULL)
        return T_NULL;

    /* Initialize all list level links to self. */
    for (int i = 0; i < TO_TIMER_SKIP_LIST_LEVEL; i++)
    {
        t_list_init(&timer->row[i]);
    }

    timer->timeout_func = timeout_func;
    timer->p = p;
    timer->init_tick = tick;
    timer->timeout_tick = 0;

    return T_OK;
}

/**
 * @brief Tick ISR hook: increments global tick, manages time slice, checks timers.
 */
void t_tick_increase(void)
{
    register t_uint32_t level;
    t_thread_t *thread;

    s_tick++;
#if (TIMER_USE_OVERFLOW_LIST)
    /* overflow, swap the list */
    if (0U == s_tick)
    {
        t_list_t *tmp = g_cur_timer_list;
        g_cur_timer_list = g_overflow_timer_list;
        g_overflow_timer_list = tmp;
    }
#endif
    /* If scheduler not started yet, nothing else to process. */
    if (t_current_thread == NULL)
        return;

    thread = t_current_thread;

    /* Decrease remaining time slice atomically. */
    level = t_irq_disable();
    --thread->remaining_tick;
    if (thread->remaining_tick == 0)/* time-slicing */
    {
        thread->remaining_tick = thread->init_tick;
        t_irq_enable(level);        
        t_thread_rotate_same_prio();
    }
    else
    {
        t_irq_enable(level);
    }

    /* Process timer expirations (callbacks executed outside critical section). */
    t_timer_check();
}

/**
 * @brief Remove timer from active list(s) if linked.
 */
t_inline void s_timer_remove(t_timer_t *timer)
{
    register t_uint32_t level = t_irq_disable();
    for (int i = 0; i < TO_TIMER_SKIP_LIST_LEVEL; i++)
    {
        t_list_delete(&timer->row[i]);
    }
    t_irq_enable(level);
}

/**
 * @brief Control timer (get/set duration).
 */
t_status_t t_timer_ctrl(t_timer_t *timer, t_uint32_t cmd, void *arg)
{
    if (timer == NULL)
        return T_NULL;

    switch (cmd)
    {
    case TO_TIMER_GET_TIME:
        if (arg)
            *(t_uint32_t *)arg = timer->init_tick;
        return T_OK;
    case TO_TIMER_SET_TIME:
        timer->init_tick = *(t_uint32_t *)arg;
        return T_OK;
    default:
        return T_UNSUPPORTED;
    }
}

/**
 * @brief Stop a running timer (removes from list).
 */
t_status_t t_timer_stop(t_timer_t *timer)
{
    if (timer == NULL)
        return T_NULL;

    s_timer_remove(timer);
    return T_OK;
}

/**
 * @brief Start (or restart) a software timer.
 * @param timer Timer object.
 */
t_status_t t_timer_start(t_timer_t *timer)
{
    if (timer == NULL)
        return T_NULL;

    register t_uint32_t level = t_irq_disable();

    /* Remove in case already active to avoid duplicate nodes. */
    s_timer_remove(timer);

    /* Compute absolute expiration (handles wrap via signed diff on check). */
    timer->timeout_tick = t_tick_get() + timer->init_tick;

    /* Ordered insertion in level 0 list by timeout_tick. */
#if (TIMER_USE_OVERFLOW_LIST)
    t_list_t *sentinel = (timer->timeout_tick > t_tick_get()) ? g_cur_timer_list : g_overflow_timer_list;
#else
    t_list_t *sentinel = g_cur_timer_list;
#endif
    t_list_t *p = sentinel;
    while (p->next != sentinel)
    {
        t_timer_t *next_timer = T_LIST_ENTRY(p->next, t_timer_t, row[0]);
#if (TIMER_USE_OVERFLOW_LIST)
        if (next_timer->timeout_tick > timer->timeout_tick)
#else
        if ((t_int32_t)(next_timer->timeout_tick - timer->timeout_tick) > 0)
#endif
            break;
        p = p->next;
    }
    t_list_insert_after(p, &timer->row[0]);

    t_irq_enable(level);
    return T_OK;
}

/**
 * @brief Scan active timers and invoke callbacks for expired timers.
 * @note Expired timers are first moved to a temp list to shorten IRQ-off window.
 */
void t_timer_check(void)
{
    register t_uint32_t level;
    t_list_t expired_list;

    t_list_init(&expired_list);

    level = t_irq_disable();
    while (!t_list_isempty(g_cur_timer_list))
    {
        t_list_t *node = g_cur_timer_list->next;
        t_timer_t *timer = T_LIST_ENTRY(node, t_timer_t, row[0]);
#if (TIMER_USE_OVERFLOW_LIST)
        if (s_tick >= timer->timeout_tick)
#else
        if ((t_int32_t)(s_tick - timer->timeout_tick) >= 0)
#endif
        {
            t_list_delete(node);
            t_list_insert_after(&expired_list, node);/* FIFO */
        }
        else
        {
            /* List ordered by timeout: stop when first non-expired. */
            break;
        }
    }
    t_irq_enable(level);

    /* Callbacks executed out of critical section to allow preemption. */
    while (!t_list_isempty(&expired_list))
    {
        t_list_t *node = expired_list.next;
        t_timer_t *timer = T_LIST_ENTRY(node, t_timer_t, row[0]);

        t_list_delete(node);

        if (timer->timeout_func)
            timer->timeout_func(timer->p);
    }
}

/**
 * @brief Default timeout callback used for thread sleep timers.
 * @param p Thread pointer.
 */
void timeout_function(void *p)
{
    t_thread_t *thread = (t_thread_t *)p;
    if (thread == NULL)
        return;

    t_list_delete(&thread->tlist);    
    thread->status = TO_THREAD_READY;
    t_sched_insert_thread(thread);    
    t_sched_switch();
}
