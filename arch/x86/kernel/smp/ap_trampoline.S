SECTION .text

GLOBAL ap_trampoline_star
GLOBAL ap_trampoline_end

; =========================================================
;  AP Trampoline — position independent
; =========================================================

ap_trampoline_start:
BITS 16
trampoline16:
    cli
    cld

    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax

    ; -----------------------------
    ; Load GDT (PIC)
    ; -----------------------------
    lea si, [cs:trampoline_gdt_desc]
    lgdt [si]

    ; -----------------------------
    ; Enter protected mode
    ; -----------------------------
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    ; far jump to flush pipeline
    jmp 0x08:trampoline32

; =========================================================
BITS 32
trampoline32:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax

    ; Enable PAE
    mov eax, cr4
    or eax, (1 << 5)
    mov cr4, eax

    ; Load CR3 (PIC)
    call get_base32
get_base32:
    pop ebx                    ; EBX = base
    mov eax, [ebx + ap_cr3 - get_base32]
    mov cr3, eax

    ; Enable long mode
    mov ecx, 0xC0000080
    rdmsr
    or eax, (1 << 8)
    wrmsr

    ; Enable paging
    mov eax, cr0
    or eax, (1 << 31)
    mov cr0, eax

    ; Jump to long mode
    jmp 0x08:trampoline64

; =========================================================
BITS 64
trampoline64:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    xor ax, ax
    mov fs, ax
    mov gs, ax

    ; Get RIP base (PIC)
    lea rbx, [rel trampoline64]

    ; Load stack
    mov rsp, [rbx + ap_stack - trampoline64]

    ; Jump to kernel entry
    mov rax, [rbx + ap_entry - trampoline64]
    jmp rax

.hang:
    hlt
    jmp .hang

; =========================================================
;  GDT
; =========================================================
ALIGN 16
trampoline_gdt:
    dq 0x0000000000000000      ; null
    dq 0x00AF9A000000FFFF      ; code 64
    dq 0x00AF92000000FFFF      ; data 64
trampoline_gdt_end:

trampoline_gdt_desc:
    dw trampoline_gdt_end - trampoline_gdt - 1
    dq trampoline_gdt          ; 64-bit base (PIC safe)

; =========================================================
;  Data filled by kernel
; =========================================================
ALIGN 8
ap_cr3:     dq 0
ap_stack:   dq 0
ap_entry:   dq 0

ALIGN 16
ap_trampoline_end:
