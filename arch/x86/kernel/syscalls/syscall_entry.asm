; global syscall_entry
; extern syscall_handler_wrapper

; section .text
; align 16
; syscall_entry:
;     swapgs
;     push r11
;     push rcx
;     push rdi
;     push rsi
;     push rdx
;     push r10
;     push r8
;     push r9

;     call syscall_handler_wrapper

;     pop r9
;     pop r8
;     pop r10
;     pop rdx
;     pop rsi
;     pop rdi
;     pop rcx
;     pop r11
;     swapgs
;     sysretq

; syscall_entry.asm
; NASM, x86_64
; Вход в ядро через SYSCALL

; global syscall_entry
; extern syscall_handler_wrapper

; section .text
; align 16
; syscall_entry:
;     swapgs                  ; переключаем GS на ядро
;     ; RSP уже должен быть установлен через TSS для SYSCALL, можно убрать mov rsp

;     ; Сохраняем регистры
;     push r11
;     push rcx
;     push rdi
;     push rsi
;     push rdx
;     push r10
;     push r8
;     push r9

;     ; Вызов обработчика syscall
;     ; В rax — номер syscall
;     ; В rdi, rsi, rdx, r10, r8, r9 — аргументы
;     call syscall_handler_wrapper

;     ; Восстанавливаем регистры
;     pop r9
;     pop r8
;     pop r10
;     pop rdx
;     pop rsi
;     pop rdi
;     pop rcx
;     pop r11

;     swapgs                  ; возвращаем GS
;     sysretq                 ; возврат в пользовательское пространство


global syscall_entry
extern syscall_handler_wrapper

section .text
align 16

syscall_entry:
    swapgs
    mov rsp, qword [gs:0]   ; kernel stack top

    push r11
    push rcx
    push rdi
    push rsi
    push rdx
    push r10
    push r8
    push r9

    ; передаём указатель на сохранённые регистры
    lea rdi, [rsp]
    call syscall_handler_wrapper

    pop r9
    pop r8
    pop r10
    pop rdx
    pop rsi
    pop rdi
    pop rcx
    pop r11
    swapgs
    sysretq

