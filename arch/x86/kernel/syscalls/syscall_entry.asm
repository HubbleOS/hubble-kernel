; -- Syscall Entry ---------------------------------------------
; Entry point for system calls via the syscall/sysret mechanism.
; Saves user registers, switches to kernel stack, calls the C
; syscall handler, restores registers, and returns to user mode.
; --------------------------------------------------------------

[BITS 64]
global syscall_entry
extern syscall_handler_wrapper
extern syscall_rsp0

section .text

%macro POP_GPRS 0
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
%endmacro

syscall_entry:
    swapgs

    ; Save user RSP
    mov     [gs:8], rsp

    ; Switch to kernel stack
    mov     rsp, [gs:0]

    ; A whole registers_t, as an interrupt would leave it: the iret part
    ; (ss, rsp, rflags, cs, rip) from the user RSP and what `syscall` put
    ; in r11 and rcx, no error code, and the syscall number as int_no (a
    ; restarted syscall needs it back). Signal delivery and execve change
    ; the frame's rip/rsp; fork reads them.
    push    qword 0x1b              ; ss
    push    qword [gs:8]            ; rsp
    push    r11                     ; rflags
    push    qword 0x23              ; cs
    push    rcx                     ; rip
    push    qword 0                 ; err_code
    push    rax                     ; int_no: the syscall number

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
    and     rsp, ~0xF

    mov     rdi, rbp
    call    syscall_handler_wrapper

    mov     rsp, rbp
    cli                             ; on the user stack below: no interrupts

    ; Nonzero: rcx/r11 must come back as they are in the frame (after
    ; rt_sigreturn), which sysret can't do (it loads rip and rflags from
    ; them): return with iretq instead.
    test    rax, rax
    jnz     .iret

    POP_GPRS
    add     rsp, 16                 ; int_no, err_code
    pop     rcx                     ; rip
    add     rsp, 8                  ; cs
    pop     r11                     ; rflags
    pop     rsp                     ; user rsp (sysret sets ss itself)
    swapgs
    o64 sysret

.iret:
    POP_GPRS
    add     rsp, 16                 ; int_no, err_code
    swapgs
    iretq
