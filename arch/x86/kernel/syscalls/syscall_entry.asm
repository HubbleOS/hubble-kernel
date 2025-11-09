; syscall_entry.asm
[BITS 64]

global syscall_entry
extern syscall_handler_wrapper

section .bss
align 16
kernel_syscall_stack: resb 8192

section .data
align 8
user_rsp_save: dq 0

section .text
align 16

syscall_entry:
    ; Сохраняем user RSP
    mov [rel user_rsp_save], rsp
    
    ; Загружаем kernel stack
    lea rsp, [rel kernel_syscall_stack + 8192]
    
    ; Эмулируем interrupt stack frame
    push qword 0x20                 ; SS
    push qword [rel user_rsp_save]  ; RSP
    push r11                        ; RFLAGS
    push qword 0x18                 ; CS
    push rcx                        ; RIP
    
    push qword 0                    ; error code
    push qword 0x80                 ; int_no
    
    ; Сохраняем все регистры
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
    
    ; Сегменты
    mov ax, ds
    push rax
    
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; Вызываем обработчик
    mov rdi, rsp
    call syscall_handler_wrapper
    
    ; Восстанавливаем
    pop rbx
    mov ds, bx
    mov es, bx
    mov fs, bx
    mov gs, bx
    
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
    add rsp, 8          ; Пропускаем RAX (результат)
    
    add rsp, 16         ; int_no + error_code
    
    ; Восстанавливаем для sysret
    pop rcx             ; RIP
    add rsp, 8          ; CS
    pop r11             ; RFLAGS
    
    ; ============================================================
    ; КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ:
    ; НЕ делаем pop rsp! Восстанавливаем из памяти:
    ; ============================================================
    mov rsp, [rel user_rsp_save]
    
    ; SYSRET вернётся в userspace
    sysretq