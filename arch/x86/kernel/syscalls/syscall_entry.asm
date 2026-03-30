[BITS 64]
global syscall_entry
extern syscall_handler_wrapper
extern syscall_rsp0

section .text

[BITS 64]
global syscall_entry
extern syscall_handler_wrapper

section .text

syscall_entry:
    swapgs

    ; save user RSP
    mov [gs:8], rsp

    ; switch to kernel stack
    mov rsp, [gs:0]

    ; save registers
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; align stack
    mov rbp, rsp
    sub rsp, 8

    mov rdi, rbp
    call syscall_handler_wrapper

    add rsp, 8
    mov rsp, rbp

    ; restore registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    ; restore user stack
    mov rsp, [gs:8]

    swapgs

    o64 sysret