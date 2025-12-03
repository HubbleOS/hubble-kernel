; ; syscall_entry.asm
; [BITS 64]

; global syscall_entry
; extern syscall_handler_wrapper

; section .bss
; align 16
; kernel_syscall_stack: resb 8192

; section .data
; align 8
; user_rsp_save: dq 0

; section .text
; align 16

; syscall_entry:
;     ; Сохраняем user RSP
;     mov [rel user_rsp_save], rsp
    
;     ; Загружаем kernel stack
;     lea rsp, [rel kernel_syscall_stack + 8192]
    
;     ; Эмулируем interrupt stack frame
;     push qword 0x20                 ; SS
;     push qword [rel user_rsp_save]  ; RSP
;     push r11                        ; RFLAGS
;     push qword 0x18                 ; CS
;     push rcx                        ; RIP
    
;     push qword 0                    ; error code
;     push qword 0x80                 ; int_no
    
;     ; Сохраняем все регистры
;     push rax
;     push rbx
;     push rcx
;     push rdx
;     push rsi
;     push rdi
;     push rbp
;     push r8
;     push r9
;     push r10
;     push r11
;     push r12
;     push r13
;     push r14
;     push r15
    
;     ; Сегменты
;     mov ax, ds
;     push rax
    
;     mov ax, 0x10
;     mov ds, ax
;     mov es, ax
;     mov fs, ax
;     mov gs, ax
    
;     ; Вызываем обработчик
;     mov rdi, rsp
;     call syscall_handler_wrapper
    
;     ; Восстанавливаем
;     pop rbx
;     mov ds, bx
;     mov es, bx
;     mov fs, bx
;     mov gs, bx
    
;     pop r15
;     pop r14
;     pop r13
;     pop r12
;     pop r11
;     pop r10
;     pop r9
;     pop r8
;     pop rbp
;     pop rdi
;     pop rsi
;     pop rdx
;     pop rcx
;     pop rbx
;     add rsp, 8          ; Пропускаем RAX (результат)
    
;     add rsp, 16         ; int_no + error_code
    
;     ; Восстанавливаем для sysret
;     pop rcx             ; RIP
;     add rsp, 8          ; CS
;     pop r11             ; RFLAGS
    
;     ; ============================================================
;     ; КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ:
;     ; НЕ делаем pop rsp! Восстанавливаем из памяти:
;     ; ============================================================
;     mov rsp, [rel user_rsp_save]
    
;     ; SYSRET вернётся в userspace
;     sysretq
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
    ; ------------------------------
    ; Сохраняем RSP пользователя
    ; ------------------------------
    mov [rel user_rsp_save], rsp

    ; ------------------------------
    ; Переключаемся на стек ядра
    ; ------------------------------
    lea rsp, [rel kernel_syscall_stack + 8192]

    ; ------------------------------
    ; Сохраняем регистры в порядке registers_t
    ; r15 ... rax
    ; ------------------------------
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

    ; ------------------------------
    ; Сохраняем сегменты
    ; ------------------------------
    mov ax, ds
    push rax
    mov ax, 0x10       ; kernel data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; ------------------------------
    ; Эмулируем interrupt frame
    ; err_code = 0, int_no = 0x80
    ; rflags = r11, CS = 0x18, RIP = RCX (SYSCALL вернул адрес следующей инструкции)
    ; ------------------------------
    push qword 0        ; err_code
    push qword 0x80     ; int_no
    push r11            ; rflags
    push qword 0x18     ; CS
    push rcx            ; RIP

    ; ------------------------------
    ; Вызов обработчика syscall
    ; Передаем указатель на registers_t
    ; ------------------------------
    mov rdi, rsp
    call syscall_handler_wrapper

    ; ------------------------------
    ; Восстанавливаем сегменты
    ; ------------------------------
    pop rax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; ------------------------------
    ; Восстанавливаем регистры в обратном порядке
    ; ------------------------------
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

    ; ------------------------------
    ; Пропускаем interrupt frame для sysret
    ; ------------------------------
    add rsp, 16          ; int_no + err_code
    pop rcx               ; RIP
    add rsp, 8            ; CS
    pop r11               ; rflags

    ; ------------------------------
    ; Восстанавливаем RSP пользователя
    ; ------------------------------
    mov rsp, [rel user_rsp_save]

    ; ------------------------------
    ; Возврат в userspace
    ; ------------------------------
    sysretq
