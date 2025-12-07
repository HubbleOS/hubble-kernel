[BITS 64]
global syscall_entry
extern syscall_handler_wrapper
extern syscall_rsp0

section .text

syscall_entry:
    swapgs
    mov [gs:0], rsp
    mov rsp, [syscall_rsp0]

    ; === Эмулировать автоматическое сохранение процессором ===
    push qword 0x1b         ; SS
    push qword [gs:0]       ; user RSP
    push r11                ; RFLAGS
    push qword 0x23         ; CS
    push rcx                ; RIP

    ; === err_code и int_no ===
    push qword 0            ; err_code
    push qword 0x80         ; int_no

    ; === Регистры в ОБРАТНОМ порядке (соответствует структуре) ===
    push rax    ; последний в структуре
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
    push r15    ; первый в структуре

    mov ax, 0x10
    mov ds, ax
    mov es, ax

    mov rbp, rsp
    and rsp, ~0xF

    mov rdi, rbp
    call syscall_handler_wrapper

    mov rsp, rbp

    ; === Восстановить в том же порядке ===
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
    pop rax     ; результат syscall (ИЗМЕНЕН wrapper'ом)

    add rsp, 16 ; int_no и err_code

    pop rcx     ; RIP
    add rsp, 8  ; CS
    pop r11     ; RFLAGS
    add rsp, 16 ; RSP + SS

    mov rsp, [gs:0]
    swapgs
    
    o64 sysret
