; -- Context Switch --------------------------------------------
; Assembly routines for task context switching.
; switch_to_task loads a new task context and performs IRETQ
; to transfer control to the new task.
; --------------------------------------------------------------

[BITS 64]

global switch_to_task
global save_context

extern lapic_eoi

; void switch_to_task(cpu_context_t *old, cpu_context_t *new)
; RDI = old context (can be NULL)
; RSI = new context
switch_to_task:

.skip_save:
    ; Load new context (RSI = new context)

    ; Restore FPU/SSE state
    mov     rax, [rsi + 200]
    test    rax, rax
    jz      .skip_fpu_load
    fxrstor [rax]

.skip_fpu_load:
    ; Restore segment selectors
    mov     ax, [rsi + 168]     ; ds
    mov     ds, ax
    mov     ax, [rsi + 176]     ; es
    mov     es, ax
    mov     ax, [rsi + 184]     ; fs
    mov     fs, ax
    mov     ax, [rsi + 192]     ; gs
    mov     gs, ax

    push    rsi                 ; save before call
    call    lapic_eoi
    pop     rsi                 ; restore after call
    sti

    ; Restore general purpose registers
    mov     r15, [rsi + 0]
    mov     r14, [rsi + 8]
    mov     r13, [rsi + 16]
    mov     r12, [rsi + 24]
    mov     r11, [rsi + 32]
    mov     r10, [rsi + 40]
    mov     r9,  [rsi + 48]
    mov     r8,  [rsi + 56]
    ; Skip RDI and RSI for now
    mov     rbp, [rsi + 80]
    mov     rbx, [rsi + 96]
    mov     rdx, [rsi + 104]
    mov     rcx, [rsi + 112]
    mov     rax, [rsi + 120]

    push    qword [rsi + 160]   ; SS
    push    qword [rsi + 128]   ; RSP
    push    qword [rsi + 144]   ; RFLAGS
    push    qword [rsi + 152]   ; CS
    push    qword [rsi + 136]   ; RIP
    mov     rdi, [rsi + 64]     ; restore rdi
    mov     rsi, [rsi + 72]     ; restore rsi LAST (kills context pointer)
    iretq
