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

    ; Stage 1: Trampoline entry (zero I/O, memory-only diagnostic)
    mov     dword [0x8226], 1

    ; Load our temporary GDT (not the kernel's yet - that comes later)
    lgdt    [0x8000 + temp_gdt_ptr - ap_trampoline_start]

    ; Enable Protected Mode (set PE bit in CR0, clear NW/CD)
    mov     eax, cr0
    or      al, 1
    and     eax, ~((1 << 29) | (1 << 30))  ; Clear NW and CD
    mov     cr0, eax

    ; Far jump to 32-bit code to flush prefetch queue
    jmp     0x08:0x8000 + protected_mode_32 - ap_trampoline_start

[BITS 32]
protected_mode_32:
    ; Stage 2: Protected-mode entry
    mov     dword [0x8226], 2

    ; Segments
    mov     ax, 0x10
    mov     ds, ax
    mov     es, ax
    mov     fs, ax
    mov     gs, ax
    mov     ss, ax
    mov     esp, 0x7C00

    ; 1. PAE + PGE (must match BSP CR4 — PGE required because kernel
    ;    page tables use global pages; without PGE the G bit is reserved
    ;    and causes a reserved-bit page fault on higher-half accesses).
    mov     eax, cr4
    or      eax, (1 << 5) | (1 << 7)
    mov     cr4, eax

    ; 2. CR3
    mov     eax, [0x8200]       ; pml4 phys
    mov     cr3, eax

    ; 3. Load the FINAL (64-bit-capable) temporary GDT now, while still
    ; in plain 32-bit protected mode with paging off. EFER.LMA is 0 at
    ; this point (it only becomes 1 once CR0.PG is set below, with LME
    ; already on), so LGDT's operand size is unambiguously 32-bit here
    ; (2-byte limit + 4-byte base, matching ap_temp_gdt_desc's layout
    ; exactly) - and this is a plain physical-address read, no page
    ; tables involved yet either way.
    ;
    ; Loading it here - before enabling long mode - instead of doing a
    ; second lgdt+retfq dance AFTER, avoids a real bug that dance had:
    ; the far jump below then lands DIRECTLY on a code descriptor with
    ; L=1, so the CPU is in true 64-bit mode (not 32-bit compatibility
    ; submode) from the very first instruction after the jump. The old
    ; two-step version jumped into "long mode" using the STILL-32-bit
    ; temp_gdt_start descriptor first (L=0), meaning everything up to
    ; its own retfq actually ran in compatibility submode despite being
    ; assembled as [BITS 64] - and REX prefixes (required for retfq's
    ; 64-bit operand size, and for `push qword`) are not recognised at
    ; all in compatibility submode. The CPU decoded that retfq's 0x48
    ; REX.W byte as the legacy opcode for `dec eax` instead, corrupting
    ; the far return.
    lgdt    [0x8000 + (ap_temp_gdt_desc - ap_trampoline_start)]

    ; 4. LME
    mov     ecx, 0xC0000080
    rdmsr
    or      eax, (1 << 8)
    wrmsr

    ; 5. PG - EFER.LMA becomes 1 the instant this is set (LME is already
    ; on), so from here on LGDT/LIDT would read a 10-byte descriptor
    ; regardless of CS.L - moot now, we don't lgdt again after this.
    mov     eax, cr0
    or      eax, (1 << 31)
    and     eax, ~((1 << 29) | (1 << 30))  ; Clear NW and CD
    mov     cr0, eax

    ; Far jump straight into TRUE 64-bit mode: selector 0x08 in the GDT
    ; just loaded is ap_temp_gdt_start's code descriptor, which has L=1.
    ; No intermediate compatibility-submode code needed at all.
    jmp     0x08:0x8000 + long_mode_64 - ap_trampoline_start

[BITS 64]
default abs
long_mode_64:
    ; Genuinely in 64-bit mode (CS.L=1) from this instruction on.

    ; Enable NXE before accessing higher-half kernel mappings with NX PTEs.
    mov     ecx, 0xC0000080
    rdmsr
    or      eax, (1 << 11)
    wrmsr

    xor     eax, eax
    mov     ds, ax
    mov     es, ax

    ; Stage 3: Long-mode entry
    mov     dword [0x8000 + (ap_data_boot_stage - ap_trampoline_start)], 3

    mov     rbx, 0x8000

    ; Load entry point using register-indirect addressing
    mov     rax, [rbx + (ap_data_entry - ap_trampoline_start)]
    test    rax, rax
    jz      .error_no_entry

    ; Load stack pointer
    mov     rsp, [0x8000 + (ap_data_stack - ap_trampoline_start)]
    test    rsp, rsp
    jz      .error_no_stack

    and     rsp, -16
    xor     rbp, rbp

    ; Save entry point before enable_sse clobbers RAX
    push    rax
    call    enable_sse
    pop     rax

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

align 16

; Sets up SSE/FXSAVE state (CR0.EM/TS/NW/CD, CR4.OSFXSR/OSXMMEXCPT) and
; runs a clean FPU init. Called once true 64-bit mode is reached, right
; before jumping to the kernel entry point.
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

; Temporary GDT for the 16-bit real mode -> 32-bit protected mode
; transition. EFER.LMA is 0 the whole time this one is in use, so its
; code segment doesn't need (and mustn't have) L=1.
temp_gdt_start:
    dq      0x0000000000000000  ; Null descriptor
    dq      0x00CF9A000000FFFF  ; Code segment (32-bit)
    dq      0x00CF92000000FFFF  ; Data segment (32-bit)
temp_gdt_end:

temp_gdt_ptr:
    dw      temp_gdt_end - temp_gdt_start - 1     ; Limit
    dd      0x8000 + temp_gdt_start - ap_trampoline_start  ; Base (physical)

; Everything from here down (code + data) must fit before offset 512:
; ap_data_start below is padded to land at exactly that offset, and
; AP_TRAMPOLINE_ADDR+512 is a hardcoded constant on the C side (smp.c's
; start_ap_callback casts it straight to `struct ap_startup_data *`) -
; see the %if size check at the end of this file, which turns any future
; overflow of this budget into a build error instead of silently
; desyncing where the BSP writes pml4_phys/stack/entry/etc. from where
; this trampoline's own (relative-offset) labels actually read them.
ap_data_area_check:

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

ap_data_boot_stage:             ; offset 550
    dd      0                   ; uint32_t boot_stage

; Temporary GDT descriptor for the 32-bit protected mode -> true 64-bit
; long mode transition, read while still in plain protected mode
; (EFER.LMA=0, so a 32-bit/6-byte LGDT read - see the comment at its use
; site above). Located at an identity-mapped address (trampoline at
; 0x8000) so LGDT can read it before paging is even on.
ap_temp_gdt_desc:
    dw      (ap_temp_gdt_end - ap_temp_gdt_start - 1)  ; limit
    dd      0x8000 + (ap_temp_gdt_start - ap_trampoline_start)  ; base (identity)

ap_temp_gdt_start:
    dq      0x0000000000000000  ; null descriptor (index 0)
    dq      0x00AF9A000000FFFF  ; 64-bit code (index 1, selector 0x08, L=1)
    dq      0x00AF92000000FFFF  ; 64-bit data (index 2, selector 0x10)
ap_temp_gdt_end:

ap_data_end:

ap_trampoline_end:

; Size checks
%if (ap_data_area_check - ap_trampoline_start) > 512
    %error "AP trampoline code before the data area exceeds the 512-byte budget - ap_data_start's offset is hardcoded as AP_TRAMPOLINE_ADDR+512 on the C side (smp.c). Growing the code before ap_data_area_check (e.g. adding debug instructions) shrinks - or, once it overflows, silently zeroes - the 'times 512-...' padding below, desyncing where the BSP writes ap_startup_data from where this file's own labels place it. If you need more room, grow the padding target and the C-side offset together, not just one of them."
%endif

%if (ap_trampoline_end - ap_trampoline_start) > 4096
    %error "Trampoline too large!"
%endif
