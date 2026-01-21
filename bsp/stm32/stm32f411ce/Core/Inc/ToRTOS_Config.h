#ifndef __TORTOS_CONFIG_H_
#define __TORTOS_CONFIG_H_

/* 1: Enable resource, 0: Disable resource */

#define TO_VERSION "1.0.0"

#define TO_LOWER_PRIORITY_NUM_HIGHER_PRIORITY   0   /* 
                                                            0:  Lower numeric value = lower priority.
                                                            1:  Lower numeric value = higher priority.
                                                        */
                                                        
#define TO_THREAD_PRIORITY_MAX      32  /* maximun is 32 */
#define TO_USING_CPU_FFS            1
#define TO_TIMER_SKIP_LIST_LEVEL    1
#define TO_TICK                     1000 /* 1000 ticks per second */

#define TO_PRINTF_BUF_SIZE          128  /* Define printf buffer size */ 

#define TO_IDLE_STACK_SIZE          256  /* define idle thread stack size */

#define TO_USING_STATIC_ALLOCATION  1
#define TO_USING_DYNAMIC_ALLOCATION 0
#if (TO_USING_DYNAMIC_ALLOCATION)
#define TO_DYNAMIC_MEM_SIZE         10240    /* bytes */
#endif

#if (0 == TO_USING_STATIC_ALLOCATION && 0 == TO_USING_DYNAMIC_ALLOCATION)
#error "At least one of TO_USING_STATIC_ALLOCATION/TO_USING_DYNAMIC_ALLOCATION must be selected."
#endif

#define TO_USING_IPC                1
#define TO_USING_MUTEX              1
#define TO_USING_RECURSIVE_MUTEX    1
#define TO_USING_SEMAPHORE          1
#define TO_USING_QUEUE              1

#define TO_DEBUG                    1

#if (1 == (TO_USING_MUTEX || TO_USING_RECURSIVE_MUTEX || TO_USING_SEMAPHORE || \
     TO_USING_QUEUE)) && (0 == TO_USING_IPC)
#error "TO_USING_IPC must be set to 1 when MUTEX/RECURSIVE_MUTEX/SEMAPHORE/QUEUE is enabled."
#endif

#if (0 == (TO_USING_MUTEX || TO_USING_RECURSIVE_MUTEX || TO_USING_SEMAPHORE || \
     TO_USING_QUEUE)) && (1 == TO_USING_IPC)
#error " When TO_USING_IPC is enabled, at least one of MUTEX/RECURSIVE_MUTEX/SEMAPHORE/QUEUE must be set to 1."
#endif

#endif /* __TORTOS_CONFIG_H_ */


