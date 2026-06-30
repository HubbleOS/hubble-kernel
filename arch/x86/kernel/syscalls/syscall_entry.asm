; ── Syscall Entry ─────────────────────────────────────────────
; Entry point for system calls via the syscall/sysret mechanism.
; Saves user registers, switches to kernel stack, calls the C
; syscall handler, restores registers, and returns to user mode.
; ──────────────────────────────────────────────────────────────

[BITS 64]
global syscall_entry
extern syscall_handler_wrapper
extern syscall_rsp0

section .text

syscall_entry:
    swapgs

    ; Save user RSP
    mov     [gs:8], rsp

    ; Switch to kernel stack
    mov     rsp, [gs:0]

    ; Save registers
    push    rax
    push    rbx
    push    rcx
    push    rdx
    push    rsi
    push    rdi
    push    rbp
    push    r8
    push    r9
    push    r10
    push    r11
    push    r12
    push    r13
    push    r14
    push    r15

    ; Align stack
    mov     rbp, rsp
    sub     rsp, 8

    mov     rdi, rbp
    call    syscall_handler_wrapper

    add     rsp, 8
    mov     rsp, rbp

    ; Restore registers
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     r11
    pop     r10
    pop     r9
    pop     r8
    pop     rbp
    pop     rdi
    pop     rsi
    pop     rdx
    pop     rcx
    pop     rbx
    pop     rax

    ; Restore user stack
    mov     rsp, [gs:8]

    swapgs

    o64 sysret
