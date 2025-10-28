; ============================================================================
; GDT, TSS, IDT Assembly Functions
; ============================================================================

[BITS 64]

global gdt_flush
global tss_flush
global idt_flush

; ----------------------------------------------------------------------------
; gdt_flush - Загружает GDT и обновляет сегментные регистры
; Аргумент: rdi = адрес gdt_ptr
; ----------------------------------------------------------------------------
gdt_flush:
    lgdt [rdi]              ; Загружаем GDT
    
    ; Обновляем сегментные регистры данных
    mov ax, 0x10            ; Kernel Data Segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Обновляем CS через far return
    pop rdi                 ; Сохраняем return address
    mov rax, 0x08           ; Kernel Code Segment
    push rax                ; Новый CS
    push rdi                ; Return address
    retfq                   ; Far return с обновлением CS

; ----------------------------------------------------------------------------
; tss_flush - Загружает TSS
; Аргумент: rdi = TSS selector (0x28)
; ----------------------------------------------------------------------------
tss_flush:
    mov ax, di
    ltr ax                  ; Load Task Register
    ret

; ----------------------------------------------------------------------------
; idt_flush - Загружает IDT
; Аргумент: rdi = адрес idt_ptr
; ----------------------------------------------------------------------------
idt_flush:
    lidt [rdi]              ; Загружаем IDT
    ret
