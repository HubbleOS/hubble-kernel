; user_enter.asm
[BITS 64]
global user_enter

; void user_enter(uint64_t entry, uint64_t stack)
; rdi = entry_point
; rsi = user_stack_top

; rdi = entry_point
; rsi = user_stack_top
; Має правильно встановити DS/ES на user data segment
user_enter:
    cli
    
    ; User data segment (0x20 | 3 = 0x23)
    mov ax, 0x23
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; Підготовка для iretq
    mov rcx, rsp        ; Зберігаємо kernel RSP
    
    push 0x23           ; SS (user data)
    push rsi            ; RSP (user stack)
    pushfq              ; RFLAGS
    or qword [rsp], 0x200  ; Enable interrupts
    push 0x1B           ; CS (user code: 0x18 | 3)
    push rdi            ; RIP
    
    iretq