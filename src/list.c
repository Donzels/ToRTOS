/**
 * @file list.c
 * @brief Intrusive doubly-linked circular list primitives.
 * @version 1.0.0
 * @date 2026-01-19
 * @author
 *   Donzel
 */

#include "tdef.h"
#include "ToRTOS.h"

/**
 * @brief Initialize list head (points to itself).
 */
void t_list_init(t_list_t *l)
{
    l->next = l->prev = l;
}

/**
 * @brief Insert node n right after node l.
 */
void t_list_insert_after(t_list_t *l, t_list_t *n)
{
    l->next->prev = n;
    n->next = l->next;
    l->next = n;
    n->prev = l;
}

/**
 * @brief Insert node n right before node l.
 */
void t_list_insert_before(t_list_t *l, t_list_t *n)
{
    l->prev->next = n;
    n->prev = l->prev;
    l->prev = n;
    n->next = l;
}

/**
 * @brief Remove node d from list and self-point it.
 */
void t_list_delete(t_list_t *d)
{
    d->next->prev = d->prev;
    d->prev->next = d->next;
    d->next = d->prev = d;
}

/**
 * @brief Check if list head is empty.
 * @return 1 if empty else 0.
 */
int t_list_isempty(t_list_t *l)
{
    return l->next == l;
}

/**
 * @brief Calculate the number of elements in a circular linked list.
 * @param l Pointer to the list head node.
 * @return The number of elements in the list.
 */
unsigned int t_list_length(t_list_t *l)
{
    unsigned int length = 0;
    t_list_t *p = l;
    while (p->next != l)
    {
        length++;
        p = p->next;
    }
    return length;
}

