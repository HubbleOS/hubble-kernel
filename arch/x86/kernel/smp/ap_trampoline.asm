; -- AP Trampoline ---------------------------------------------
; Application Processor startup code. This code is copied to
; physical address 0x8000. APs start execution here in 16-bit
; real mode, transition through protected mode to long mode,
; and finally jump to the kernel entry point.
; --------------------------------------------------------------

[BITS 16]

section .text.ap_trampoline

global ap_trampoline_start
global ap_trampoline_end

ap_trampoline_start:
    cli                         ; Disable interrupts
    cld                         ; Clear direction flag

    ; Setup segments to zero
    xor     ax, ax
    mov     ds, ax
    mov     es, ax
    mov     ss, ax
    mov     sp, 0x7C00          ; Temporary stack in real mode

    ; Load our temporary GDT (not the kernel's yet - that comes later)
    lgdt    [0x8000 + temp_gdt_ptr - ap_trampoline_start]

    ; Enable Protected Mode (set PE bit in CR0)
    mov     eax, cr0
    or      al, 1
    mov     cr0, eax

    ; Far jump to 32-bit code to flush prefetch queue
    jmp     0x08:0x8000 + protected_mode_32 - ap_trampoline_start

[BITS 32]
protected_mode_32:
    ; Segments
    mov     ax, 0x10
    mov     ds, ax
    mov     es, ax
    mov     fs, ax
    mov     gs, ax
    mov     ss, ax
    mov     esp, 0x7C00

    ; 1. PAE
    mov     eax, cr4
    or      eax, (1 << 5)
    mov     cr4, eax

    ; 2. CR3
    mov     eax, [0x8200]       ; pml4 phys
    mov     cr3, eax

    ; 3. LME
    mov     ecx, 0xC0000080
    rdmsr
    or      eax, (1 << 8)
    wrmsr

    ; 4. PG
    mov     eax, cr0
    or      eax, (1 << 31)
    mov     cr0, eax

    ; Now we're in compatibility mode, jump to 64-bit code
    jmp     0x08:0x8000 + long_mode_64 - ap_trampoline_start

[BITS 64]
long_mode_64:
    xor     eax, eax
    mov     ds, ax
    mov     es, ax

    ; Step 1: Load TEMPORARY GDT from identity-mapped trampoline area.
    ; CPU is still in 32-bit compatibility mode (CS.L=0, CS.D=1), so
    ; lgdt reads a 6-byte descriptor (2-byte limit + 4-byte base).
    lgdt    [0x8000 + (ap_temp_gdt_desc - ap_trampoline_start)]

    ; Far return to true 64-bit long mode via temp GDT's 64-bit code segment
    push    0x08
    push    qword (0x8000 + (.load_kernel_gdt - ap_trampoline_start))
    retfq

.load_kernel_gdt:
    mov     rbx, 0x8000

    ; Step 2: Load REAL kernel GDT. Now in true 64-bit mode (CS.L=1),
    ; lgdt reads the full 10-byte descriptor (2-byte limit + 8-byte base).
    lgdt    [rbx + (ap_data_gdt_desc - ap_trampoline_start)]

    ; Reload CS with kernel GDT's 64-bit code segment
    push    0x08
    push    qword (0x8000 + (.reload_cs - ap_trampoline_start))
    retfq

.reload_cs:
    ; Set up data segments from kernel GDT
    mov     ax, 0x10
    mov     ds, ax
    mov     es, ax
    mov     ss, ax

    call    enable_sse

    ; Load entry point using register-indirect addressing
    mov     rax, [0x8000 + (ap_data_entry - ap_trampoline_start)]
    test    rax, rax
    jz      .error_no_entry

    ; Load stack pointer
    mov     rsp, [0x8000 + (ap_data_stack - ap_trampoline_start)]
    test    rsp, rsp
    jz      .error_no_stack

    and     rsp, -16
    xor     rbp, rbp

    ; Signal ready
    mov     dword [rbx + (ap_data_ready - ap_trampoline_start)], 1

    ; Jump to kernel entry point
    jmp     rax

.error_no_stack:
    ; Stack pointer was zero
    mov     dword [0x8000 + (ap_data_ready - ap_trampoline_start)], 0xDEAD0001
    jmp     .hang

.error_no_entry:
    ; Entry point was zero
    mov     dword [0x8000 + (ap_data_ready - ap_trampoline_start)], 0xDEAD0002
    jmp     .hang

.hang:
    cli
    hlt
    jmp     .hang

; Temporary GDT for transitioning to protected/long mode
; This is a minimal flat GDT just to get into 64-bit mode
align 16

enable_sse:
    mov     rax, cr0

    ; Explicitly clear
    btr     rax, 2              ; EM = 0
    btr     rax, 3              ; TS = 0
    btr     rax, 29             ; NW = 0
    btr     rax, 30             ; CD = 0

    ; Explicitly set
    bts     rax, 1              ; MP = 1
    bts     rax, 5              ; NE = 1

    mov     cr0, rax
    clts                        ; Mandatory

    mov     rax, cr4
    or      rax, (1 << 9) | (1 << 10)   ; OSFXSR | OSXMMEXCPT
    mov     cr4, rax

    fninit                      ; Now without fault
    ret

temp_gdt_start:
    dq      0x0000000000000000  ; Null descriptor
    dq      0x00CF9A000000FFFF  ; Code segment (32-bit)
    dq      0x00CF92000000FFFF  ; Data segment (32-bit)
temp_gdt_end:

temp_gdt_ptr:
    dw      temp_gdt_end - temp_gdt_start - 1     ; Limit
    dd      0x8000 + temp_gdt_start - ap_trampoline_start  ; Base (physical)

; Pad to offset 512 for data area
times 512 - ($ - ap_trampoline_start) db 0

; Data area - filled by BSP before starting AP
ap_data_start:

ap_data_pml4:
    dq      0                   ; uint64_t pml4_phys (offset 512)

ap_data_gdt_desc:               ; offset 520
    dw      0                   ; uint16_t gdt_limit
    dq      0                   ; uint64_t gdt_base (physical)

ap_data_stack:                  ; offset 530
    dq      0                   ; uint64_t stack_top

ap_data_entry:                  ; offset 538
    dq      0                   ; uint64_t entry_point

ap_data_ready:                  ; offset 546
    dd      0                   ; uint32_t ap_ready

; Temporary GDT descriptor for the 32→64 bit mode transition.
; Located at an identity-mapped address (trampoline at 0x8000) so that
; lgdt in 32-bit compatibility mode can read it (2-byte limit + 4-byte base).
ap_temp_gdt_desc:               ; offset 550
    dw      (ap_temp_gdt_end - ap_temp_gdt_start - 1)  ; limit
    dd      0x8000 + (ap_temp_gdt_start - ap_trampoline_start)  ; base (identity)

ap_temp_gdt_start:              ; offset 556
    dq      0x0000000000000000  ; null descriptor (index 0)
    dq      0x00AF9A000000FFFF  ; 64-bit code (index 1, selector 0x08)
    dq      0x00AF92000000FFFF  ; 64-bit data (index 2, selector 0x10)
ap_temp_gdt_end:

ap_data_end:

ap_trampoline_end:

; Size check
%if (ap_trampoline_end - ap_trampoline_start) > 4096
    %error "Trampoline too large!"
%endif
