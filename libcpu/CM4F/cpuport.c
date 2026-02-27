/**
 * @file cpuport.c
 * @brief Cortex-M3 CPU port: stack frame initialization & ffs helper.
 * @version 1.0.0
 * @date 2026-01-19
 * author
 *   Donzel
 */

#include "ToRTOS.h"

#define INITIAL_XPSR       0x01000000UL   /* Thumb mode bit set in xPSR */
#define INITIAL_EXC_RETURN 0xFFFFFFFDUL   /* Return to Thread mode, use PSP, standard frame (bit4=1: no FPU context) */

/**
 * @brief Saved register frame (software stacked + hardware stacked).
 * Layout matches push/pop sequence for context switch.
 */
typedef struct
{
    t_uint32_t r4;
    t_uint32_t r5;
    t_uint32_t r6;
    t_uint32_t r7;
    t_uint32_t r8;
    t_uint32_t r9;
    t_uint32_t r10;
    t_uint32_t r11;
    t_uint32_t exc_return;

    /* Hardware-stacked on exception entry */
    t_uint32_t r0;
    t_uint32_t r1;
    t_uint32_t r2;
    t_uint32_t r3;
    t_uint32_t r12;
    t_uint32_t lr;
    t_uint32_t pc;
    t_uint32_t psr;
} t_stack_t;

/**
 * @brief Initialize a thread stack frame (Cortex-M PSP layout).
 * @param stackaddr Top address of stack (end of buffer).
 * @param entry Thread entry function.
 * @param arg Argument of entry.
 * @return PSP pointer after context frame prepared.
 */
t_uint8_t *t_stack_init(t_uint8_t *stackaddr, t_thread_entry_t entry, void *arg)
{
    t_stack_t *pstack;
    t_uint8_t *top_of_stack;
    t_uint8_t i;

    top_of_stack = stackaddr;

    /* 8-byte align per ARM procedure call & exception entry requirements. */
    top_of_stack = (t_uint8_t *)(((t_uint32_t)top_of_stack) & ~((8) - 1));

    /* Reserve space for initial context frame */
    top_of_stack -= sizeof(t_stack_t);
    pstack = (t_stack_t *)top_of_stack;

    /* Clear frame area */
    for (i = 0; i < 16; i++)
        ((t_uint32_t *)pstack)[i] = 0;

    pstack->psr = INITIAL_XPSR;         /* Default xPSR (Thumb bit set) */
    pstack->pc  = (t_uint32_t)entry;    /* Entry point */
    pstack->r0  = (t_uint32_t)arg;
    pstack->lr  = (t_uint32_t)t_thread_exit; /* If thread function returns */

    pstack->exc_return = INITIAL_EXC_RETURN;

    return top_of_stack;
}

#if (TO_LOWER_PRIORITY_NUM_HIGHER_PRIORITY)
#if TO_USING_CPU_FFS
/* Architecture-specific __t_ffs provided in assembly/inline blocks below. */
#if defined(__CC_ARM)
__asm int __t_ffs(int value)
{
    CMP     r0, #0x00
    BEQ     exit
    RBIT    r0, r0
    CLZ     r0, r0
    ADDS    r0, r0, #0x01
exit
    BX      lr
}
#elif defined(__CLANG_ARM)
int __t_ffs(int value)
{
    __asm volatile(
        "CMP     r0, #0x00            \n"
        "BEQ     1f                   \n"
        "RBIT    r0, r0               \n"
        "CLZ     r0, r0               \n"
        "ADDS    r0, r0, #0x01        \n"
        "1:                           \n"
        : "=r"(value)
        : "r"(value)
    );
    return value;
}
#elif defined(__IAR_SYSTEMS_ICC__)
int __t_ffs(int value)
{
    if (value == 0) 
        return value;
    asm("RBIT %0, %1" : "=r"(value) : "r"(value));
    asm("CLZ  %0, %1" : "=r"(value) : "r"(value));
    asm("ADDS %0, %1, #0x01" : "=r"(value) : "r"(value));
    return value;
}
#elif defined(__GNUC__)
int __t_ffs(int value)
{
    return __builtin_ffs(value);
}
#endif
#else/* TO_USING_CPU_FFS */
/**
 * @brief Generic fallback implementation of __t_ffs (first set bit).
 */
int __t_ffs(int value)
{
#if defined(__GNUC__) || defined(__CLANG_ARM)
    return __builtin_ffs(value);
#else
    if (value == 0) 
        return 0;
    int idx = 1;
    while ((value & 1) == 0) 
        { value >>= 1; ++idx; }
    return idx;
#endif
}
#endif /* TO_USING_CPU_FFS */
#else/* TO_LOWER_PRIORITY_NUM_HIGHER_PRIORITY */
#if TO_USING_CPU_FFS
#if defined(__CC_ARM)  /* ARM Compiler 5 */
__asm int __t_fls(int value)
{
    CMP     r0, #0
    BEQ     no_bit
    CLZ     r1, r0        // count leading zeros
    MOV     r0, #32
    SUB     r0, r0, r1    // 32 - clz(value) = highest set bit index
    BX      lr
no_bit
    BX      lr
}

#elif defined(__CLANG_ARM)  /* ARM Compiler 6 (Clang-based) */
static inline int __t_fls(int value)
{
    int result;
    __asm volatile(
        "CMP     %1, #0        \n"   // if value == 0
        "BEQ     1f            \n"   // branch to return 0
        "CLZ     %0, %1        \n"   // result = leading zeros
        "MOV     r2, #32       \n"
        "SUB     %0, r2, %0    \n"   // result = 32 - CLZ
        "B       2f            \n"
        "1:                    \n"
        "MOV     %0, #0        \n"
        "2:                    \n"
        : "=r"(result)
        : "r"(value)
        : "r2"
    );
    return result;
}

#elif defined(__IAR_SYSTEMS_ICC__)  /* IAR */
static inline int __t_fls(int value)
{
    if (value == 0) 
        return 0;
    int lz;
    __asm("CLZ %0, %1" : "=r"(lz) : "r"(value));
    return 32 - lz;
}

#elif defined(__GNUC__)  /* GCC / Clang */
static inline int __t_fls(int value)
{
    if (value == 0) 
        return 0;
    return 32 - __builtin_clz(value);
}

#else  /* Generic fallback */
static inline int __t_fls(int value)
{
    if (value == 0) 
        return 0;
    int idx = 32;
    while ((value & (1UL << idx)) == 0) 
        { --idx; }
    return idx;
}
#endif
#endif /* TO_USING_CPU_FFS */  
#endif /* TO_LOWER_PRIORITY_NUM_HIGHER_PRIORITY */
