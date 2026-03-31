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
    lgdt [rdi]
    
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Оновлюємо CS через far return
    pop rax                 ;  Витягуємо return address
    mov rcx, 0x08          ; Kernel Code Segment
    push rcx               ; Push новий CS
    push rax               ; Push return address
    retfq                  ; Far return

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
