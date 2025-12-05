[BITS 64]

global syscall_entry
extern syscall_handler_wrapper

section .bss
align 16
kernel_syscall_stack: resb 8192

section .data
align 8
user_rsp_save: dq 0
user_rcx_save: dq 0
user_r11_save: dq 0

section .text
align 16

syscall_entry:
    ; Save critical syscall registers to memory immediately
    mov [rel user_rcx_save], rcx    ; Return RIP
    mov [rel user_r11_save], r11    ; Return RFLAGS
    mov [rel user_rsp_save], rsp    ; User stack

    lea rsp, [rel kernel_syscall_stack + 8192]

    ; Save registers
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rbp
    push rdi
    push rsi
    push rdx
    push rcx
    push rbx
    push rax

    ; Save segments
    mov ax, ds
    push rax
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Build interrupt frame
    push qword 0        ; err_code
    push qword 0x80     ; int_no
    push qword [rel user_r11_save]  ; RFLAGS
    push qword 0x1B     ; CS
    push qword [rel user_rcx_save]  ; RIP

    ; Call handler
    mov rdi, rsp
    call syscall_handler_wrapper

    ; Restore segments
    pop rax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Restore registers
    pop rax
    pop rbx
    pop rcx
    pop rdx
    pop rsi
    pop rdi
    pop rbp
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15

    ; Skip interrupt frame fields
    add rsp, 16         ; int_no + err_code

    ; Get saved return values from memory (NOT from stack!)
    mov rcx, [rel user_rcx_save]
    mov r11, [rel user_r11_save]
    mov r10, [rel user_rsp_save]

    ; Validate RFLAGS
    or r11, 0x202       ; Set IF and reserved bit 1

    ; Build IRETQ frame
    push qword 0x23     ; SS
    push r10            ; RSP
    push r11            ; RFLAGS
    push qword 0x1B     ; CS
    push rcx            ; RIP

    iretq