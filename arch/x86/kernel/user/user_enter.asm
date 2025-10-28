; user_enter.asm
[BITS 64]
global user_enter

; void user_enter(uint64_t entry, uint64_t stack)
; rdi = entry_point
; rsi = user_stack_top

user_enter:
    cli                         ; Отключаем прерывания на время настройки

    mov ax, 0x20                ; user data selector (Ring 3)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Подготовка стека для iretq
    push qword 0x20             ; SS (user data selector)
    push qword rsi              ; RSP (user stack top)
    pushfq                      ; RFLAGS
    push qword 0x18             ; CS (user code selector)
    push qword rdi              ; RIP (entry point)

    iretq                       ; Перейти в user space (CPL=3)
