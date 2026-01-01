[BITS 64]

global switch_to_task
global save_context
global load_context

extern lapic_eoi

; void switch_to_task(cpu_context_t *old, cpu_context_t *new)
; RDI = old context (can be NULL)
; RSI = new context
switch_to_task:
    ; Save old context if provided
    test rdi, rdi
    jz .skip_save
    
    ; Save all general purpose registers
    mov [rdi + 0],   r15
    mov [rdi + 8],   r14
    mov [rdi + 16],  r13
    mov [rdi + 24],  r12
    mov [rdi + 32],  r11
    mov [rdi + 40],  r10
    mov [rdi + 48],  r9
    mov [rdi + 56],  r8
    mov [rdi + 64],  rdi
    mov [rdi + 72],  rsi
    mov [rdi + 80],  rbp
    ; [rdi + 88] = unused
    mov [rdi + 96],  rbx
    mov [rdi + 104], rdx
    mov [rdi + 112], rcx
    mov [rdi + 120], rax
    
    ; Save RSP
    mov [rdi + 128], rsp
    
    ; Save RIP (return address)
    mov rax, [rsp]
    mov [rdi + 136], rax
    
    ; Save segment selectors
    mov ax, cs
    mov [rdi + 144], ax
    mov ax, ss
    mov [rdi + 146], ax
    mov ax, ds
    mov [rdi + 148], ax
    mov ax, es
    mov [rdi + 150], ax
    mov ax, fs
    mov [rdi + 152], ax
    mov ax, gs
    mov [rdi + 154], ax
    
    ; Save RFLAGS
    pushfq
    pop rax
    mov [rdi + 156], rax
    
    ; Save FPU/SSE state
    mov rax, [rdi + 164]  ; Get fpu_state pointer
    test rax, rax
    jz .skip_save
    fxsave [rax]

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
    
    call lapic_eoi
    sti
    ; Jump to new RIP
    ret

; Helper to save just the context without switching
save_context:
    ; Similar to above but only saves
    mov [rdi + 0],   r15
    ; ... (same as switch_to_task save part)
    ret

; Helper to load context without saving
load_context:
    ; Similar to above but only loads
    mov r15, [rdi + 0]
    ; ... (same as switch_to_task load part
    ret