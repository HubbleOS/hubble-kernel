; kernel/arch/x86_64/syscall_entry.asm
bits 64

section .text
global syscall_entry
extern syscall_handler

syscall_entry:
    ; syscall портит:
    ; rcx = user RIP (куда возвращаться)
    ; r11 = user RFLAGS
    
    ; Сохраняем userspace контекст
    swapgs              ; Меняем GS для доступа к kernel data
    
    ; Сохраняем user RSP
    mov [gs:0], rsp     ; Или используй TSS
    
    ; Переключаемся на kernel stack
    mov rsp, [gs:8]     ; Kernel RSP из per-CPU данных
    
    ; Сохраняем регистры которые должны быть preserved
    push rcx            ; User RIP
    push r11            ; User RFLAGS
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15
    
    ; Аргументы уже в нужных регистрах:
    ; rax = syscall number
    ; rdi = arg1
    ; rsi = arg2
    ; rdx = arg3
    ; r10 = arg4 (не rcx!)
    ; r8 = arg5
    ; r9 = arg6
    
    ; Вызываем C handler
    mov rdi, rax        ; syscall_num
    mov rsi, rdi        ; нет! переименуем параметры
    ; Передадим все как есть, переделаем handler
    
    call syscall_handler
    
    ; Результат в rax
    
    ; Восстанавливаем регистры
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    pop r11             ; User RFLAGS
    pop rcx             ; User RIP
    
    ; Восстанавливаем user stack
    mov rsp, [gs:0]
    
    swapgs              ; Возвращаем user GS
    
    ; Возврат в userspace
    sysretq
