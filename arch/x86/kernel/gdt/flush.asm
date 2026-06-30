; ── GDT, TSS, IDT Flush Functions ─────────────────────────────
; Assembly wrappers for loading the GDT, TSS, and IDT
; descriptor registers.
; ──────────────────────────────────────────────────────────────

[BITS 64]

global gdt_flush
global tss_flush
global idt_flush

; ── gdt_flush ──────────────────────────────────────────────────
; Loads the GDT and updates all segment registers.
; Argument: rdi = address of gdt_ptr
; ────────────────────────────────────────────────────────────────
gdt_flush:
    lgdt    [rdi]

    mov     ax, 0x10
    mov     ds, ax
    mov     es, ax
    mov     fs, ax
    mov     gs, ax
    mov     ss, ax

    ; Reload CS via far return
    pop     rax                 ; extract return address
    mov     rcx, 0x08           ; Kernel Code Segment
    push    rcx                 ; push new CS
    push    rax                 ; push return address
    retfq                       ; far return

; ── tss_flush ──────────────────────────────────────────────────
; Loads the Task State Segment.
; Argument: rdi = TSS selector (0x28)
; ────────────────────────────────────────────────────────────────
tss_flush:
    mov     ax, di
    ltr     ax                  ; Load Task Register
    ret

; ── idt_flush ──────────────────────────────────────────────────
; Loads the Interrupt Descriptor Table.
; Argument: rdi = address of idt_ptr
; ────────────────────────────────────────────────────────────────
idt_flush:
    lidt    [rdi]               ; load IDT
    ret
