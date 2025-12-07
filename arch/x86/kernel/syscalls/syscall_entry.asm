[BITS 64]
global syscall_entry
extern syscall_handler
extern syscall_rsp0

section .text

syscall_entry:
    ; Сохраняем user RSP во временной переменной
    mov qword [rel user_rsp_temp], rsp
    
    ; Загружаем kernel stack
    mov rsp, [rel syscall_rsp0]
    
    ; Создаем iretq-совместимый фрейм
    push qword 0x1B         ; SS (User Data)
    push qword [rel user_rsp_temp]  ; User RSP
    push r11                ; RFLAGS (сохраненные процессором в R11)
    push qword 0x23         ; CS (User Code)
    push rcx                ; RIP (сохранен процессором в RCX)
    
    ; Сохраняем все регистры (КРОМЕ R11 и RCX - они уже в стековом фрейме!)
    push rax
    push rbx
    push rdx                ; НЕ push rcx - он уже сохранен!
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    ; НЕ push r11 - он уже сохранен как RFLAGS!
    push r12
    push r13
    push r14
    push r15
    
    ; Устанавливаем kernel сегменты
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    
    ; Вызываем обработчик: syscall_handler(num, a1, a2, a3, a4, a5, a6)
    mov rdi, rax            ; num (было в RAX)
    ; rsi = a1 (уже на месте)
    ; rdx = a2 (уже на месте)
    mov rcx, r10            ; a3 (было в R10)
    ; r8 = a4 (уже на месте)
    ; r9 = a5 (уже на месте)
    
    ; Выравниваем стек
    mov rbp, rsp
    and rsp, ~0xF
    call syscall_handler
    mov rsp, rbp
    
    ; Сохраняем результат
    mov [rsp], rax          ; Перезаписываем сохраненный RAX результатом
    
    ; Восстанавливаем регистры
    pop r15
    pop r14
    pop r13
    pop r12
    ; Пропускаем R11 - восстановим из RFLAGS позже
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rbx
    pop rax                 ; Результат syscall
    
    ; Подготавливаем возврат для SYSRET
    pop rcx                 ; RIP
    add rsp, 8              ; Пропускаем CS
    pop r11                 ; RFLAGS
    or r11, 0x200           ; Устанавливаем IF (Interrupt Flag)
    
    pop rsp                 ; User RSP
    
    ; Устанавливаем User Data сегменты
    mov dx, 0x1B
    mov ds, dx
    mov es, dx
    
    ; Возврат в userspace
    sysret

section .data
align 8
user_rsp_temp: dq 0
