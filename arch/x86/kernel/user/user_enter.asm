[BITS 64]
global user_enter

user_enter:
    cli

    ; Сохраняем аргументы
    mov rcx, rdi                ; entry point
    mov r11, rsi                ; stack top

    ; Выравниваем стек ядра
    and rsp, ~0xF

    ; Устанавливаем сегменты данных
    mov ax, 0x1B
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Строим IRETQ frame
    push qword 0x1B    ; SS = User Data (index 3 = 0x18 | 3)
    push r11           ; RSP
    pushfq
    pop rax
    or rax, 0x200
    push rax           ; RFLAGS
    push qword 0x23    ; CS = User Code (index 4 = 0x20 | 3)
    push rcx           ; RIP

    ; Обнуляем регистры
    xor rax, rax
    xor rbx, rbx
    xor rcx, rcx
    xor rdx, rdx
    xor rsi, rsi
    xor rdi, rdi
    xor rbp, rbp
    xor r8, r8
    xor r9, r9
    xor r10, r10
    xor r11, r11
    xor r12, r12
    xor r13, r13
    xor r14, r14
    xor r15, r15

    iretq
    ud2
