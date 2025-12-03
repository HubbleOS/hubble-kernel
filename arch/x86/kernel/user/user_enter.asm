; ; user_enter.asm
; [BITS 64]
; global user_enter

; ; void user_enter(uint64_t entry, uint64_t stack)
; ; rdi = entry_point
; ; rsi = user_stack_top

; ; Має правильно встановити DS/ES на user data segment
; user_enter:
;     cli
    
;     ; User data segment (0x20 | 3 = 0x23)
;     mov ax, 0x23
;     mov ds, ax
;     mov es, ax
;     mov fs, ax
;     mov gs, ax
    
;     ; Підготовка для iretq
;     ; mov rcx, rsp        ; Зберігаємо kernel RSP
;     mov rsp, rsi
    
;     push 0x23           ; SS (user data)
;     push rsi            ; RSP (user stack)
;     pushfq              ; RFLAGS
;     or qword [rsp], 0x200  ; Enable interrupts
;     push 0x1B           ; CS (user code: 0x18 | 3)
;     push rdi            ; RIP
    
;     iretq

; user_enter в assembly

[BITS 64]
global user_enter
user_enter:
    ; rdi = entry point
    ; rsi = stack top
    
    cli
    
    ; Готуємо user mode segments
    mov ax, 0x23        ; User Data Segment (0x20 | 3)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; Готуємо стек для iretq
    push 0x23           ; SS (User Data | RPL3)
    push rsi            ; RSP (user stack)
    pushfq              ; RFLAGS
    pop rax
    or rax, 0x200       ; IF = 1 (enable interrupts)
    push rax
    push 0x1B           ; CS (User Code | RPL3) ✅ 0x18 | 3 = 0x1B
    push rdi            ; RIP (entry point)
    
    ; Очищаємо регістри
    xor rax, rax
    xor rbx, rbx
    xor rcx, rcx
    xor rdx, rdx
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


; [BITS 64]
; global user_enter

; ; rdi = entry_point
; ; rsi = user_stack_top

; user_enter:
;     cli

;     ; User data segment
;     mov ax, 0x23
;     mov ds, ax
;     mov es, ax
;     mov fs, ax
;     mov gs, ax

;     ; iretq frame
;     push 0x23       ; SS
;     push rsi        ; RSP
;     pushfq
;     pop r11
;     or r11, 0x200   ; enable IF
;     push r11
;     push 0x1B       ; CS
;     push rdi        ; RIP
;     iretq
