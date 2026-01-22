/**
 * @file ipc.c
 * @brief IPC primitives: semaphore, mutex, message queue and suspend helpers.
 * @version 1.0.0
 * @date 2026-01-19
 * @author
 *   Donzel
 */

#include "ToRTOS.h"

#if TO_USING_IPC
/**
 * @brief Suspend a thread into an IPC wait list (FIFO or PRIO).
 * @param sentinel Suspend list sentinel.
 * @param thread Thread to suspend.
 * @param flag TO_IPC_FLAG_FIFO or TO_IPC_FLAG_PRIO.
 */
t_status_t t_ipc_suspend(t_list_t *sentinel, t_thread_t *thread, t_uint8_t flag)
{
    register t_uint32_t level;
    t_list_t *p;

    if (!sentinel || !thread)
        return T_NULL;

    /* enter critical */
    level = t_irq_disable();

    /* remove from ready queue (if any) and mark blocked */
    t_sched_remove_thread(thread);
    thread->status = TO_THREAD_SUSPEND;    

    /* insert into suspend list according to flag */
    switch (flag)
    {
    case TO_IPC_FLAG_FIFO:
        t_list_insert_before(sentinel, &thread->tlist);
        break;
    case TO_IPC_FLAG_PRIO: /* PRIO */
        p = sentinel;
        while (p->next != sentinel)
        {
            t_thread_t *next_thread = T_LIST_ENTRY(p->next, t_thread_t, tlist);
#if (TO_LOWER_PRIORITY_NUM_HIGHER_PRIORITY)            
            if (thread->current_priority < next_thread->current_priority)
#else
            if (thread->current_priority > next_thread->current_priority)
#endif
                break;
            p = p->next;
        }
        t_list_insert_before(p, &thread->tlist);
        break;
    default:
        /* Unsupported flag -> append FIFO style */
        t_list_insert_before(sentinel, &thread->tlist);
        break;
    }

    t_irq_enable(level);
    return T_OK;
}

/**
 * @brief Resume all threads in given suspend list (no immediate schedule).
 * @param sentinel Suspend list sentinel.
 * @note Caller may invoke t_sched_switch() if desired.
 */
t_status_t t_ipc_list_resume_all(t_list_t *sentinel)
{
    register t_uint32_t level;
    t_thread_t *thread;

    if (!sentinel)
        return T_NULL;

    while (!t_list_isempty(sentinel))
    {
        level = t_irq_disable();
        thread = T_LIST_ENTRY(sentinel->next, t_thread_t, tlist);
        t_list_delete(&thread->tlist);
        thread->status = TO_THREAD_READY;
        t_sched_insert_thread(thread);
        t_irq_enable(level);
    }
    return T_OK;
}

/* Delete an IPC object and wake waiting threads */
t_status_t t_ipc_delete(t_ipc_t *ipc)
{
    if (!ipc) 
        return T_NULL;
    if (0 == ipc->status) 
        return T_OK;

    if (!t_list_isempty(&ipc->wait_list))
    {
        t_ipc_list_resume_all(&ipc->wait_list);
        t_sched_switch();
    }

    ipc->status = 0;
    ipc->msg_waiting = 0;
    ipc->length = 0;
    ipc->item_size = 0;

#if ((1 == TO_USING_DYNAMIC_ALLOCATION) && (0 == TO_USING_STATIC_ALLOCATION))
    t_free(ipc);     
#endif
#if (TO_USING_STATIC_ALLOCATION && TO_USING_DYNAMIC_ALLOCATION)
    if(!ipc->is_static_allocated)
    {
        t_free(ipc);     
    }   
#endif    

    return T_OK;
}
/* Main IPC object (used for semaphore / mutex / queue) */
#if TO_USING_SEMAPHORE
#if (TO_USING_STATIC_ALLOCATION)
t_status_t t_sema_create_static(t_uint16_t max_count, t_uint16_t init_count, t_uint8_t mode, t_ipc_t *ipc)
{
    if (!max_count || !ipc) 
        return T_NULL;

    t_list_init(&ipc->wait_list);

    ipc->type = IPC_SEMA;
    ipc->status = 1;
    ipc->mode = mode;
    ipc->msg_waiting = 0;
    ipc->length = max_count;
    ipc->item_size = 0;

    ipc->u.sema.holder = NULL;
    ipc->u.sema.recursive = 0;

    ipc->msg_waiting = init_count; 

#if (TO_USING_STATIC_ALLOCATION && TO_USING_DYNAMIC_ALLOCATION)
    ipc->is_static_allocated = 1;
#endif      
    
    return T_OK;
}
#endif /* TO_USING_STATIC_ALLOCATION */
#if (TO_USING_DYNAMIC_ALLOCATION)
t_status_t t_sema_create(t_uint16_t max_count, t_uint16_t init_count, t_uint8_t mode, t_ipc_t **ipc_handle)
{
    if (!max_count) 
        return T_NULL;
    t_ipc_t *ipc = t_malloc(sizeof(t_ipc_t));
    if(!ipc)
        return T_ERR;

    t_list_init(&ipc->wait_list);

    ipc->type = IPC_SEMA;
    ipc->status = 1;
    ipc->mode = mode;
    ipc->msg_waiting = 0;
    ipc->length = max_count;
    ipc->item_size = 0;

    ipc->u.sema.holder = NULL;
    ipc->u.sema.recursive = 0;

    ipc->msg_waiting = init_count; 

#if (TO_USING_STATIC_ALLOCATION && TO_USING_DYNAMIC_ALLOCATION)
    ipc->is_static_allocated = 0;
#endif  

    if(ipc_handle)
        *ipc_handle = ipc;   
    return T_OK;
}
#endif /* TO_USING_DYNAMIC_ALLOCATION */

t_status_t t_sema_send(t_ipc_t *ipc)
{
    register t_uint32_t level;
    t_uint8_t need_schedule = 0;

    if (!ipc) 
        return T_NULL;
    if (0 == ipc->status) 
        return T_DELETED;
    if(IPC_SEMA != ipc->type)
        return T_INVALID;   

    level = t_irq_disable();

    if (0 == ipc->status)
    {
        t_irq_enable(level);
        return T_DELETED;
    }
    if (ipc->msg_waiting < ipc->length)
    {
        ipc->msg_waiting++;
        if (!t_list_isempty(&ipc->wait_list))
        {
            t_thread_t *th = T_LIST_ENTRY(ipc->wait_list.next, t_thread_t, tlist);
            t_list_delete(&th->tlist);
            th->status = TO_THREAD_READY;
            t_sched_insert_thread(th);
            need_schedule = 1;
        }
        t_irq_enable(level);
        if (need_schedule)
            t_sched_switch();
        return T_OK;
    }
    return T_ERR;
}

t_status_t t_sema_recv(t_ipc_t *ipc, t_int32_t timeout)
{
    register t_uint32_t level;
    t_uint32_t start_tick = 0;

    if (!ipc) 
        return T_NULL;
    if (0 == ipc->status) 
        return T_DELETED;   
    if(IPC_SEMA != ipc->type)
        return T_INVALID;   
    while (1)
    {
        level = t_irq_disable();

        if (0 == ipc->status)
        {
            t_irq_enable(level);
            return T_DELETED;
        } 
        if (ipc->msg_waiting > 0)
        {
            ipc->msg_waiting--;
            t_irq_enable(level);
            return T_OK;
        }

        if (0 == timeout)
        {
            t_irq_enable(level);
            return T_ERR;
        }

        /* suspend current thread */
        t_ipc_suspend(&ipc->wait_list, t_current_thread, ipc->mode);       

        /* Start timer if timeout specified */
        if (timeout > 0 && TO_WAITING_FOREVER != timeout)
        {
            if (start_tick == 0)
                start_tick = t_tick_get();
            t_timer_ctrl(&t_current_thread->timer, TO_TIMER_SET_TIME, &timeout);
            t_timer_start(&t_current_thread->timer);
        }

        t_irq_enable(level);
        t_sched_switch();   /* thread will sleep */

        /* ---- after wake up ---- */
        if (0 == ipc->status)
            return T_DELETED;

        /* check timeout */
        if (timeout > 0 && TO_WAITING_FOREVER != timeout)
        {
            t_uint32_t now = t_tick_get();
            t_uint32_t elapsed = get_tick_diff(start_tick, now);
            if (elapsed >= timeout)
                return T_ERR;
            timeout -= elapsed;
            start_tick = now;
        }
        /* retry */
        continue;
    }             
}

#endif
#if (TO_USING_MUTEX || TO_USING_RECURSIVE_MUTEX)

#if (TO_USING_STATIC_ALLOCATION)
t_status_t t_mutex_create_static_base(t_ipc_type_t ipc_type, t_uint8_t mode, t_ipc_t *ipc)
{
    if (!ipc) 
        return T_NULL;

    t_list_init(&ipc->wait_list);

    ipc->type = ipc_type;
    ipc->status = 1;
    ipc->mode = mode;
    ipc->msg_waiting = 0;
    ipc->length = 1;
    ipc->item_size = 0;

    ipc->u.sema.holder = NULL;
    ipc->u.sema.recursive = 0;
    ipc->u.sema.original_prio = DUMMY_PRIORITY;

    ipc->msg_waiting = 1; /* mutex available */

#if (TO_USING_STATIC_ALLOCATION && TO_USING_DYNAMIC_ALLOCATION)
    ipc->is_static_allocated = 1;
#endif       
    
    return T_OK;
}   
#endif /* TO_USING_STATIC_ALLOCATION */

#if (TO_USING_DYNAMIC_ALLOCATION)
t_status_t t_mutex_create_base(t_ipc_type_t ipc_type, t_uint8_t mode, t_ipc_t **ipc_handle)
{
    t_ipc_t *ipc = t_malloc(sizeof(t_ipc_t));
    if(!ipc)
        return T_ERR;

    t_list_init(&ipc->wait_list);

    ipc->type = ipc_type;
    ipc->status = 1;
    ipc->mode = mode;
    ipc->msg_waiting = 0;
    ipc->length = 1;
    ipc->item_size = 0;

    ipc->u.sema.holder = NULL;
    ipc->u.sema.recursive = 0;
    ipc->u.sema.original_prio = DUMMY_PRIORITY;

    ipc->msg_waiting = 1; /* mutex available */

#if (TO_USING_STATIC_ALLOCATION && TO_USING_DYNAMIC_ALLOCATION)
    ipc->is_static_allocated = 0;
#endif       
    if(ipc_handle)
        *ipc_handle = ipc;
    
    return T_OK;
}   
#endif /* TO_USING_DYNAMIC_ALLOCATION */


t_status_t t_mutex_send_base(t_ipc_t *ipc)
{
    register t_uint32_t level;
    t_uint32_t start_tick = 0;
    (void)start_tick;
    t_uint8_t need_schedule = 0;

    if (!ipc) 
        return T_NULL;
    if (0 == ipc->status) 
        return T_DELETED;

    level = t_irq_disable();

    if (0 == ipc->status)
    {
        t_irq_enable(level);
        return T_DELETED;
    }
    /* Only owner can release */
    if (t_current_thread != ipc->u.sema.holder)
    {
        t_irq_enable(level);
        return T_ERR;
    }
#if (TO_USING_RECURSIVE_MUTEX)
    if(IPC_RECURSIVE_MUTEX == ipc->type)
    {
        /* Decrease recursion count */
        if (ipc->u.sema.recursive > 0)
            ipc->u.sema.recursive--;

        /* Still held by recursion */
        if (ipc->u.sema.recursive > 0)
        {
            t_irq_enable(level);
            return T_OK;
        }
    }   
#endif

    /* Fully release mutex */
    ipc->msg_waiting = 1;
    ipc->u.sema.holder = NULL;

    /* --- Restore priority --- */
    if (ipc->u.sema.original_prio != DUMMY_PRIORITY &&
        t_current_thread->current_priority != ipc->u.sema.original_prio)
    {
        t_thread_ctrl(t_current_thread, TO_THREAD_SET_PRIORITY,
                    &ipc->u.sema.original_prio);
        ipc->u.sema.original_prio = DUMMY_PRIORITY;
    }

    /* Wake one waiting thread */
    if (!t_list_isempty(&ipc->wait_list))
    {
        t_thread_t *th = T_LIST_ENTRY(ipc->wait_list.next, t_thread_t, tlist);
        t_list_delete(&th->tlist);
        th->status = TO_THREAD_READY;
        t_sched_insert_thread(th);
        need_schedule = 1;
    }

    t_irq_enable(level);
    if (need_schedule)
        t_sched_switch();
    return T_OK;    
}

t_status_t t_mutex_recv_base(t_ipc_t *ipc, t_int32_t timeout)
{
    register t_uint32_t level;
    t_uint32_t start_tick = 0;

    if (!ipc) 
        return T_NULL;
    if (0 == ipc->status) 
        return T_DELETED;  
    while (1)
    {
        level = t_irq_disable();

        if (0 == ipc->status)
        {
            t_irq_enable(level);
            return T_DELETED;
        }
        if (1 == ipc->msg_waiting)
        {
            ipc->msg_waiting = 0;
            ipc->u.sema.holder = t_current_thread;
            ipc->u.sema.recursive = 1;
            ipc->u.sema.original_prio = t_current_thread->current_priority;
            t_irq_enable(level);
            return T_OK;
        }

        if (ipc->u.sema.holder == t_current_thread)
        {
#if (TO_USING_RECURSIVE_MUTEX) 
            if(IPC_RECURSIVE_MUTEX == ipc->type)
                ipc->u.sema.recursive++;
#endif
            t_irq_enable(level);
            return T_OK;
        }

        if (0 == timeout)
        {
            t_irq_enable(level);
            return T_ERR;
        }

        /* apply priority inheritance if needed */
#if TO_LOWER_PRIORITY_NUM_HIGHER_PRIORITY
        if (ipc->u.sema.holder &&
            t_current_thread->current_priority < ipc->u.sema.holder->current_priority)
#else
        if (ipc->u.sema.holder &&
            t_current_thread->current_priority > ipc->u.sema.holder->current_priority)
#endif
        {
            if (DUMMY_PRIORITY == ipc->u.sema.original_prio)
                ipc->u.sema.original_prio = ipc->u.sema.holder->current_priority;
            t_thread_ctrl(ipc->u.sema.holder,
                            TO_THREAD_SET_PRIORITY,
                            &t_current_thread->current_priority);
        }

        /* suspend and start timer */
        t_ipc_suspend(&ipc->wait_list, t_current_thread, ipc->mode);
        if (timeout > 0 && TO_WAITING_FOREVER != timeout)
        {
            if (start_tick == 0)
                start_tick = t_tick_get();
            t_timer_ctrl(&t_current_thread->timer, TO_TIMER_SET_TIME, &timeout);
            t_timer_start(&t_current_thread->timer);
        }

        t_irq_enable(level);
        t_sched_switch();

        /* after wake */
        if (0 == ipc->status)
            return T_DELETED;
        if (ipc->u.sema.holder == t_current_thread)
            return T_OK;

        if (timeout > 0 && TO_WAITING_FOREVER != timeout)
        {
            t_uint32_t now = t_tick_get();
            t_uint32_t elapsed = get_tick_diff(start_tick, now);
            if (elapsed >= timeout)
                return T_ERR;
            timeout -= elapsed;
            start_tick = now;
        }
        continue;
    }
      
}

#endif

#if TO_USING_QUEUE
static void __t_memcpy(t_uint8_t *dst, const t_uint8_t *src, t_uint16_t len)
{
    while (len--)
        *dst++ = *src++;
}

#if (TO_USING_STATIC_ALLOCATION)
t_status_t t_queue_create_static(void *queue_pool, t_uint16_t queue_length, t_uint16_t item_size, t_uint8_t mode, t_ipc_t *ipc)
{
    if (!queue_pool || !item_size || !queue_length || !ipc) 
        return T_NULL;

    t_list_init(&ipc->wait_list);

    ipc->type = IPC_QUEUE;
    ipc->status = 1;
    ipc->mode = mode;
    ipc->msg_waiting = 0;
    ipc->length = queue_length;
    ipc->item_size = item_size;

    t_uint8_t *base = (t_uint8_t *)queue_pool;
    ipc->u.queue.head = base;
    ipc->u.queue.tail = base + (item_size * queue_length);
    ipc->u.queue.write_to = ipc->u.queue.head;
    ipc->u.queue.read_from = ipc->u.queue.head;

#if (TO_USING_STATIC_ALLOCATION && TO_USING_DYNAMIC_ALLOCATION)
    ipc->is_static_allocated = 1;
#endif        

    return T_OK;
}
#endif /* TO_USING_STATIC_ALLOCATION */

#if (TO_USING_DYNAMIC_ALLOCATION)
t_status_t t_queue_create(t_uint16_t queue_length, t_uint16_t item_size, t_uint8_t mode, t_ipc_t **ipc_handle)
{
    if (!item_size || !queue_length) 
        return T_NULL;
    t_ipc_t *ipc = t_malloc(sizeof(t_ipc_t));
    if(!ipc)
        return T_ERR;
    void *queue_pool = t_malloc(item_size * queue_length);
    if(!queue_pool)
    {
        t_free(ipc);
        return T_ERR;
    }

    t_list_init(&ipc->wait_list);

    ipc->type = IPC_QUEUE;
    ipc->status = 1;
    ipc->mode = mode;
    ipc->msg_waiting = 0;
    ipc->length = queue_length;
    ipc->item_size = item_size;

    t_uint8_t *base = (t_uint8_t *)queue_pool;
    ipc->u.queue.head = base;
    ipc->u.queue.tail = base + (item_size * queue_length);
    ipc->u.queue.write_to = ipc->u.queue.head;
    ipc->u.queue.read_from = ipc->u.queue.head;

#if (TO_USING_STATIC_ALLOCATION && TO_USING_DYNAMIC_ALLOCATION)
    ipc->is_static_allocated = 0;
#endif    

    if(ipc_handle)
        *ipc_handle = ipc;

    return T_OK;    
}
#endif /* TO_USING_DYNAMIC_ALLOCATION */

t_status_t t_queue_send(t_ipc_t *ipc, const void *data, t_int32_t timeout)
{
    register t_uint32_t level;
    t_uint32_t start_tick = 0;
    t_uint8_t need_schedule = 0;

    if (!ipc) 
        return T_NULL;
    if (0 == ipc->status) 
        return T_DELETED;    
    if(IPC_QUEUE != ipc->type)
        return T_INVALID;
    while (1)
    {
        if (ipc->msg_waiting < ipc->length)
        {
            /* Write data to queue */
            __t_memcpy(ipc->u.queue.write_to, data, ipc->item_size);
            ipc->u.queue.write_to += ipc->item_size;
            if (ipc->u.queue.write_to >= ipc->u.queue.tail)
                ipc->u.queue.write_to = ipc->u.queue.head;
            ipc->msg_waiting++;

            /* Wake one receiver if waiting */
            if (!t_list_isempty(&ipc->wait_list))
            {
                t_thread_t *rth = T_LIST_ENTRY(ipc->wait_list.next, t_thread_t, tlist);
                t_list_delete(&rth->tlist);
                rth->status = TO_THREAD_READY;
                t_sched_insert_thread(rth);
                need_schedule = 1;
            }
            t_irq_enable(level);
            if (need_schedule)
                t_sched_switch();
            return T_OK;
        }

        /* Queue is full */
        if (0 == timeout)
        {
            t_irq_enable(level);
            return T_ERR;
        }

        if (!t_current_thread)
        {
            t_irq_enable(level);
            return T_UNSUPPORTED;
        }

        /* Suspend current thread */
        t_ipc_suspend(&ipc->wait_list, t_current_thread, ipc->mode);

        /* Setup timeout timer */
        if (timeout > 0 && TO_WAITING_FOREVER != timeout)
        {
            if (start_tick == 0)
                start_tick = t_tick_get();
            t_timer_ctrl(&t_current_thread->timer, TO_TIMER_SET_TIME, &timeout);
            t_timer_start(&t_current_thread->timer);
        }

        t_irq_enable(level);
        t_sched_switch();

        /* After wake up, check status and timeout */
        if (0 == ipc->status)
            return T_DELETED;

        if (timeout > 0 && TO_WAITING_FOREVER != timeout)
        {
            t_uint32_t now = t_tick_get();
            t_uint32_t elapsed = get_tick_diff(start_tick, now);
            if (elapsed >= timeout)
                return T_ERR;
            timeout -= (t_int32_t)elapsed;
            start_tick = now;
        }

        /* Retry after wait */
        continue;
    }
    
}

t_status_t t_queue_recv(t_ipc_t *ipc, void *data, t_int32_t timeout)
{
    register t_uint32_t level;
    t_uint32_t start_tick = 0;

    if (!ipc) 
        return T_NULL;
    if (0 == ipc->status) 
        return T_DELETED;  
    if(IPC_QUEUE != ipc->type)
        return T_INVALID;
    while (1)
    {
        level = t_irq_disable();

        if (0 == ipc->status)
        {
            t_irq_enable(level);
            return T_DELETED;
        }
        if (ipc->msg_waiting > 0)
        {
            /* read one item */
            __t_memcpy(data, ipc->u.queue.read_from, ipc->item_size);
            ipc->u.queue.read_from += ipc->item_size;
            if (ipc->u.queue.read_from >= ipc->u.queue.tail)
                ipc->u.queue.read_from = ipc->u.queue.head;                
            ipc->msg_waiting--;

            /* wake one sender */
            if (!t_list_isempty(&ipc->wait_list))
            {
                t_thread_t *sth = T_LIST_ENTRY(ipc->wait_list.next, t_thread_t, tlist);
                t_list_delete(&sth->tlist);
                sth->status = TO_THREAD_READY;
                t_sched_insert_thread(sth);
            }
            t_irq_enable(level);
            t_sched_switch();
            return T_OK;
        }

        if (0 == timeout)
        {
            t_irq_enable(level);
            return T_ERR;
        }

        /* queue empty: suspend */
        t_ipc_suspend(&ipc->wait_list, t_current_thread, ipc->mode);

        /* start timer if needed */
        if (timeout > 0 && TO_WAITING_FOREVER != timeout)
        {
            if (start_tick == 0)
                start_tick = t_tick_get();
            t_timer_ctrl(&t_current_thread->timer, TO_TIMER_SET_TIME, &timeout);
            t_timer_start(&t_current_thread->timer);
        }

        t_irq_enable(level);
        t_sched_switch();

        /* after wake: check again */
        if (0 == ipc->status)
            return T_DELETED;

        if (timeout > 0 && TO_WAITING_FOREVER != timeout)
        {
            t_uint32_t now = t_tick_get();
            t_uint32_t elapsed = get_tick_diff(start_tick, now);
            if (elapsed >= timeout)
                return T_ERR;
            timeout -= elapsed;
            start_tick = now;
        }
        continue;
    }
      
}

#endif

#endif /* TO_USING_IPC */
