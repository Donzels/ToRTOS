;******************************************************************************
; File: arm.s
; Brief: Cortex-M4F context switch & IRQ primitives (PendSV + FPU support)
; Version: 1.0.0
; Date: 2026-01-19
; Author: Donzel
;------------------------------------------------------------------------------
; Key Features:
;   - Cortex-M4F optimized context switch (supports FPU registers S16-S31)
;   - PendSV-based thread context save/restore (lowest priority exception)
;   - PRIMASK-based IRQ enable/disable primitives
;   - FPU initialization and context handling
;   - Systick/PendSV priority configuration
;------------------------------------------------------------------------------

    ; Import global variables (defined in C code)
    IMPORT  t_prev_thread_sp_p          ; Pointer to previous thread's SP storage
    IMPORT  t_next_thread_sp_p          ; Pointer to next thread's SP storage
    IMPORT  t_interrupt_flag            ; Context switch flag (0=idle, 1=pending)

;------------------------------
; Core Peripheral Register Definitions (Cortex-M4F SCB/FPU)
;------------------------------
SCB_VTOR_REG        EQU     0xE000ED08  ; Vector table offset register    
SCB_ICSR_REG        EQU     0xE000ED04  ; Interrupt Control and State Register
SCB_PENDSVSET       EQU     0x10000000  ; PendSV set bit (write 1 to trigger)
SCB_SHPR3_REG       EQU     0xE000ED20  ; System Handler Priority Register 3
SCB_SYSTICK_PRI     EQU     0xF0000000  ; SysTick priority (lowest: 0xF << 24)
SCB_PENDSV_PRI      EQU     0x00F00000  ; PendSV priority (lowest: 0xF << 16)

FPU_CPACR_REG       EQU     0xE000ED88  ; Coprocessor Access Control Register (enable FPU)
FPU_FULL_ACCESS     EQU     0x00F00000  ; Full Access FPU (CP10/CP11 full access: 0xF << 20)

    ; Code section configuration
    AREA |.text|, CODE, READONLY, ALIGN=2   ; Read-only code section, 2-byte alignment
    THUMB                                   ; Use THUMB instruction set (required for Cortex-M)
    REQUIRE8                                ; Require 8-byte stack alignment
    PRESERVE8                               ; Preserve 8-byte stack alignment

;------------------------------
; Function: t_irq_disable
; Purpose: Disable global interrupts (PRIMASK) and return previous PRIMASK state
; Prototype: t_uint32_t t_irq_disable(void)
; Return: r0 = Previous PRIMASK value (0=interrupts enabled, 1=disabled)
;------------------------------
t_irq_disable    PROC
    EXPORT  t_irq_disable            ; Export function for C code linkage
             
    mrs     r0, PRIMASK              ; Read PRIMASK register into r0 (save current state)
    cpsid   i                        ; Disable IRQs (set PRIMASK=1)
    bx      lr                       ; Return to caller (r0 holds old PRIMASK)
    ENDP

;------------------------------
; Function: t_irq_enable
; Purpose: Restore global interrupt state from saved PRIMASK value
; Prototype: void t_irq_enable(t_uint32_t disirq)
; Param: r0 = Saved PRIMASK value (from t_irq_disable)
;------------------------------
t_irq_enable     PROC
    EXPORT  t_irq_enable             ; Export function for C code linkage
             
    msr     PRIMASK, r0              ; Restore PRIMASK from r0 (re-enable IRQs if needed)
    bx      lr                       ; Return to caller
    ENDP

;------------------------------
; Function: t_first_switch_task
; Purpose: Initialize and trigger the FIRST thread context switch (no previous thread)
; Prototype: void t_first_switch_task(uint32_t to)
; Param: r0 = Pointer to the stack pointer (PSP) of the first thread to run
; Notes: 
;   - Only called once (at OS startup)
;   - Enables FPU, configures priorities, triggers PendSV for first context switch
;------------------------------
t_first_switch_task PROC
    EXPORT t_first_switch_task       ; Export function for C code linkage
    PRESERVE8                        ; Ensure stack alignment for FPU operations

    ldr     r1, =t_next_thread_sp_p
    str     r0, [r1]                 ; Store 'to' PSP pointer container

    ldr     r1, =t_prev_thread_sp_p
    mov     r0, #0
    str     r0, [r1]                 ; Mark no previous

    ldr     r1, =t_interrupt_flag
    mov     r0, #1
    str     r0, [r1]                 ; Set switch flag    
    ;-----------------------------
    ; Initialize thread pointer variables for first switch
    ;-----------------------------
    ldr     r0, =SCB_SHPR3_REG       ; Load address of SHPR3 (SysTick/PendSV priority)
    ldr     r2, [r0]                 ; Read current SHPR3 value
 
    ldr     r1, =SCB_SYSTICK_PRI     ; Load SysTick priority mask
    ldr     r3, =SCB_PENDSV_PRI      ; Load PendSV priority mask
    orr     r1, r1, r3               ; Merge SysTick and PendSV masks
 
    orr     r1, r1, r2               ; Merge with existing priority values
    str     r1, [r0]                 ; Write back new priority values to SHPR3

    ;-----------------------------
    ; Set SysTick and PendSV to LOWEST priority
    ;-----------------------------
    ldr     r0, =SCB_SHPR3_REG       ; Load address of SHPR3 (SysTick/PendSV priority)
    ldr     r2, [r0]                 ; Read current SHPR3 value
    ldr     r1, =SCB_SYSTICK_PRI     ; Load SysTick priority mask
    orr     r1, r1, r2               ; Merge with existing priority values
    ldr     r1, =SCB_PENDSV_PRI      ; Load PendSV priority mask
    orr     r1, r1, r2               ; Merge with existing priority values (keep other bits)
    str     r1, [r0]                 ; Write back new priority values to SHPR3
    
    ;-----------------------------
    ; Enable FPU (Coprocessor 10/11) - Only needs to run once
    ;-----------------------------
    ldr     r0, =FPU_CPACR_REG       ; Load address of CPACR register
    ldr     r1, [r0]                 ; Read current CPACR value
    ldr     r2, =FPU_FULL_ACCESS     ; Load FPU full access mask
    orr     r1, r1, r2               ; Enable CP10 and CP11 (FPU): bits 20-23 = 0b1111
    str     r1, [r0]                 ; Write back to enable FPU
    dsb                              ; Data Synchronization Barrier (ensure write completes)
    isb                              ; Instruction Synchronization Barrier (flush pipeline)

    ;-----------------------------
    ; Reset MSP to vector table initial value (safety measure)
    ;-----------------------------
    ldr     r0, =SCB_VTOR_REG        ; Load address of SCB->VTOR (Vector Table Offset Register)
    ldr     r0, [r0]                 ; Read VTOR to get base address of vector table
    ldr     r0, [r0]                 ; Read first entry of vector table (initial MSP value)
    msr     MSP, r0                  ; Set Main Stack Pointer (MSP) to reset value

    ;-----------------------------
    ; Clear CONTROL register (ensure FPCA bit is cleared)
    ; FPCA (bit 2) = 0: No floating-point context active before first switch
    ;-----------------------------
    mov     r0, #0                   ; Load 0 into r0
    msr     CONTROL, r0              ; Write 0 to CONTROL register
    isb                              ; Instruction barrier (ensure register write takes effect)

    ;-----------------------------
    ; Trigger PendSV exception to perform first context switch
    ;-----------------------------
    ldr     r0, =SCB_ICSR_REG        ; Load address of NVIC ICSR register
    ldr     r1, =SCB_PENDSVSET       ; Load PendSV set bit mask
    str     r1, [r0]                 ; Write to ICSR to trigger PendSV
    dsb                              ; Data barrier (ensure write completes)
    isb                              ; Instruction barrier (flush pipeline)

    ;-----------------------------
    ; Enable global interrupts (IRQ + FIQ)
    ;-----------------------------
    cpsie   i                        ; Enable IRQs (clear PRIMASK)
    cpsie   f                        ; Enable FIQs (clear FAULTMASK)

    bx      lr                       ; Return to caller (PendSV will execute next)
    ENDP

;------------------------------
; Function: t_normal_switch_task
; Purpose: Trigger a NORMAL thread context switch (save current thread, load next)
; Prototype: void t_normal_switch_task(uint32_t from, uint32_t to)
; Params:
;   r0 = Pointer to current (from) thread's SP storage
;   r1 = Pointer to next (to) thread's SP storage
; Notes:
;   - Sets switch flag and triggers PendSV (actual switch happens in PendSV_Handler)
;   - Prevents re-entrant switches with interrupt_flag
;------------------------------
t_normal_switch_task PROC
    EXPORT t_normal_switch_task     ; Export function for C code linkage

    ; Check if context switch is already pending
    ldr     r2, =t_interrupt_flag   ; Load address of switch flag
    ldr     r3, [r2]                ; Read current flag value
    CMP     r3, #1                  ; Compare to 1 (switch pending)
    BEQ     _reswitch               ; If already pending, skip to set next thread

    ; First time entering switch: set flag and save previous thread
    mov     r3, #1                  ; Set switch flag to 1
    str     r3, [r2]                ; Store flag value

    ldr     r2, =t_prev_thread_sp_p ; Load address of previous thread SP pointer
    str     r0, [r2]                ; Save current thread's SP storage address

_reswitch
    ; Set next thread's SP storage address
    ldr     r2, =t_next_thread_sp_p ; Load address of next thread SP pointer
    str     r1, [r2]                ; Store next thread's SP storage address

    ; Trigger PendSV exception to perform context switch
    ldr     r0, =SCB_ICSR_REG       ; Load address of NVIC ICSR register
    ldr     r1, =SCB_PENDSVSET      ; Load PendSV set bit mask
    str     r1, [r0]                ; Write to ICSR to trigger PendSV
    bx      lr                      ; Return to caller (switch happens in PendSV)
    ENDP

;------------------------------
; Function: PendSV_Handler
; Purpose: Core context switch logic (save/restore thread state including FPU)
; Trigger: Software-triggered via SCB_PENDSVSET bit (lowest priority exception)
; Notes:
;   - Runs in Handler mode (uses MSP)
;   - Saves/restores callee-saved registers (r4-r11) + FPU registers (S16-S31)
;   - Uses PSP (Process Stack Pointer) for thread stacks
;------------------------------
PendSV_Handler   PROC
    EXPORT PendSV_Handler           ; Export handler for vector table linkage
    PRESERVE8                       ; Ensure stack alignment for FPU operations
            
    ; Check if context switch is pending (interrupt_flag)
    ldr     r0, =t_interrupt_flag   ; Load address of switch flag
    ldr     r1, [r0]                ; Read flag value
    cbz     r1, pendsv_exit         ; If flag=0 (no switch), exit immediately

    ; Clear switch flag (mark switch as in progress)
    mov     r1, #0                  ; Set flag to 0
    str     r1, [r0]                ; Store cleared flag

    ; Check if there's a previous thread to save (skip for first switch)
    ldr     r0, =t_prev_thread_sp_p ; Load address of previous thread SP pointer
    ldr     r3, [r0]                ; Read pointer to previous thread's SP storage
    cbz     r3, switch_to_thread    ; If r3=0 (first switch), skip save -> load next thread

    ; Save current thread's context (integer + FPU registers)
    mrs     r1, psp                 ; Read current PSP (Process Stack Pointer) = old thread's SP
    isb                             ; Instruction barrier (ensure PSP read is valid)

    ; Check if FPU context is active (EXC_RETURN bit 4 = 0 -> FP regs stacked)
    tst     lr, #0x10               ; Test bit 4 of LR (EXC_RETURN value)
    it      eq                      ; If bit 4=0 (EQ), execute next instruction
    vstmdbeq r1!, {s16-s31}         ; Save FPU registers S16-S31 to stack (increment SP after)

    ; Save integer callee-saved registers (r4-r11) + LR (for exception return)
    stmdb   r1!, {r4-r11, lr}       ; Store r4-r11 + LR to stack (decrement SP before)

    ; Save updated PSP back to previous thread's SP storage
    ldr     r0, [r0]                ; Read address of previous thread's SP storage
    str     r1, [r0]                ; Write new PSP (stack pointer) to storage

; Restore next thread's context
switch_to_thread
    ; Load next thread's PSP from storage
    ldr     r1, =t_next_thread_sp_p ; Load address of next thread SP pointer
    ldr     r1, [r1]                ; Read pointer to next thread's SP storage
    ldr     r1, [r1]                ; Read actual PSP value (next thread's stack pointer)

    ; Restore integer callee-saved registers (r4-r11) + LR
    ldmia   r1!, {r4-r11, lr}       ; Load r4-r11 + LR from stack (increment SP after)

    ; Restore FPU registers if active (EXC_RETURN bit 4 = 0)
    tst     lr, #0x10               ; Test bit 4 of LR (EXC_RETURN value)
    it      eq                      ; If bit 4=0 (EQ), execute next instruction
    vldmiaeq r1!, {s16-s31}         ; Load FPU registers S16-S31 from stack

    ; Update PSP to next thread's stack pointer
    msr     psp, r1                 ; Write new SP to PSP
    isb                             ; Instruction barrier (ensure PSP write is valid)

; Exit PendSV handler
pendsv_exit

    ; Configure LR for exception return to Thread mode (using PSP)
    orr     lr, lr, #0x04           ; Set bit 2 of LR (EXC_RETURN) -> use PSP after return
    bx      lr                      ; Return from exception (switch to next thread)
    ENDP

    ALIGN   4                       ; Ensure 4-byte alignment for handler
    END                             ; End of assembly file