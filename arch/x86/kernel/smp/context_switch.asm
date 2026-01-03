[BITS 64]

global switch_to_task
global save_context
global load_context

extern lapic_eoi

; void switch_to_task(cpu_context_t *old, cpu_context_t *new)
; RDI = old context (can be NULL)
; RSI = new context
switch_to_task:


.skip_save:
    ; Load new context (RSI = new context)
    
    ; Restore FPU/SSE state
    mov rax, [rsi + 164]
    test rax, rax
    jz .skip_fpu_load
    fxrstor [rax]

.skip_fpu_load:
    ; out 0x3f8, 0x20
    ; Restore segment selectors
    mov ax, [rsi + 148]
    mov ds, ax
    mov ax, [rsi + 150]
    mov es, ax
    mov ax, [rsi + 152]
    mov fs, ax
    mov ax, [rsi + 154]
    mov gs, ax

    call lapic_eoi
    sti
    
    ; Restore general purpose registers
    mov r15, [rsi + 0]
    mov r14, [rsi + 8]
    mov r13, [rsi + 16]
    mov r12, [rsi + 24]
    mov r11, [rsi + 32]
    mov r10, [rsi + 40]
    mov r9,  [rsi + 48]
    mov r8,  [rsi + 56]
    ; Skip RDI and RSI for now
    mov rbp, [rsi + 80]
    mov rbx, [rsi + 96]
    mov rdx, [rsi + 104]
    mov rcx, [rsi + 112]
    mov rax, [rsi + 120]
    
    ; Restore RSP
    mov rsp, [rsi + 128]
    
    ; Push new RIP onto stack for ret
    push qword [rsi + 136]

    ; Restore RDI and RSI last
    mov rdi, [rsi + 64]
    mov rsi, [rsi + 72]    
    ; Jump to new RIP
    ret



; Helper to load context without saving
load_context:
    ; Similar to above but only loads
    mov r15, [rdi + 0]
    ; ... (same as switch_to_task load part
    ret