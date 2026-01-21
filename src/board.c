/**
 * @file board.c
 * @brief Board-level startup helpers (banner, idle thread).
 * @version 1.0.0
 * @date 2026-01-19
 * author
 *   Donzel
 */

#include "ToRTOS.h"

#define TO_BUILD_DATE __DATE__ " " __TIME__
#define TO_COPYRIGHT "Copyright (c) 2026 ToRTOS Project"

/**
 * @brief Weak banner printer (can be overridden by user).
 */
__weak void t_start_banner(void)
{
    t_printf("\r\n");
    t_printf("=================================================\r\n");
    t_printf("  ToRTOS - Lightweight Real-Time Operating System\r\n");
    t_printf("  Version    : %s\r\n", TO_VERSION);
#if TO_DEBUG
    t_printf("  Build Date : %s\r\n", TO_BUILD_DATE);
#endif
    t_printf("  %s\r\n", TO_COPYRIGHT);
    t_printf("=================================================\r\n");
    t_printf("\r\n");
}

/* Idle thread objects (statically allocated) */
#if (TO_USING_STATIC_ALLOCATION)
static t_thread_t idle_thread_instance;
static t_thread_t *idle_thread_handle = &idle_thread_instance;
static t_uint8_t idle_stack[TO_IDLE_STACK_SIZE];
#else
static t_thread_t *idle_thread_handle;
#endif
static volatile int idle_counter = 0;

/**
 * @brief Idle thread entry: performs background cleanup & optional power saving.
 */
// static void idle_thread_entry(void)
static void idle_thread_entry(void *arg)
{
    while (1)
    {
        /* Reclaim waiting_termination threads. */
        t_cleanup_waiting_termination_threads();

        /* Optionally insert low-power instruction (WFI). */
        /* __asm volatile ("wfi"); */
    }
}

/**
 * @brief Initialize and start idle thread (lowest priority).
 */
static t_status_t t_idle_thread_init(void)
{
#if (TO_USING_STATIC_ALLOCATION)    
    t_status_t ret = t_thread_create_static(idle_thread_entry,
                                   idle_stack,
                                   TO_IDLE_STACK_SIZE,
#if (TO_LOWER_PRIORITY_NUM_HIGHER_PRIORITY)
                                   TO_THREAD_PRIORITY_MAX - 1,
#else
                                   0,
#endif
                                   NULL,
                                   5,
                                   &idle_thread_instance);    
#endif
#if ((1 == TO_USING_DYNAMIC_ALLOCATION) && (0 == TO_USING_STATIC_ALLOCATION))    
    t_status_t ret = t_thread_create(idle_thread_entry,
                                   TO_IDLE_STACK_SIZE,
#if (TO_LOWER_PRIORITY_NUM_HIGHER_PRIORITY)
                                   TO_THREAD_PRIORITY_MAX - 1,
#else
                                   0,
#endif
                                   NULL,
                                   5,
                                   &idle_thread_handle);    
#endif                                                                  
    if (ret != T_OK)
        return ret;
    return t_thread_startup(idle_thread_handle);
}

/**
 * @brief Initialize kernel core subsystems.
 */
t_status_t t_tortos_init(void)
{
    t_sched_init();
    t_timer_list_init();
    t_idle_thread_init();
    t_start_banner();
    return T_OK;
}
