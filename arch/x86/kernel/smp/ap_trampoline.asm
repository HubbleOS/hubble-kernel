; ap_trampoline.asm - Application Processor startup code
; This code is copied to physical address 0x8000
; APs start here in 16-bit real mode

[BITS 16]

section .text.ap_trampoline

global ap_trampoline_start
global ap_trampoline_end

ap_trampoline_start:
    cli                         ; Disable interrupts
    cld                         ; Clear direction flag
    
    ; Setup segments to zero
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00              ; Temporary stack in real mode
    
    ; Load our temporary GDT (not the kernel's yet - that comes later)
    lgdt [0x8000 + temp_gdt_ptr - ap_trampoline_start]
    
    ; Enable Protected Mode (set PE bit in CR0)
    mov eax, cr0
    or al, 1
    mov cr0, eax
    
    ; Far jump to 32-bit code to flush prefetch queue
    jmp 0x08:0x8000 + protected_mode_32 - ap_trampoline_start

[BITS 32]
protected_mode_32:
    ; Setup 32-bit segments (0x10 = data segment)
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax


    
    ; Setup temporary 32-bit stack
    mov esp, 0x7C00
    
    ; Enable PAE (Physical Address Extension)
    mov eax, cr4
    or eax, 0x20                ; Set PAE bit (bit 5)
    mov cr4, eax
    
    ; Load PML4 physical address into CR3
    mov eax, [0x8200]           ; pml4_phys at offset 512
    mov cr3, eax
    
    ; Enable Long Mode in EFER MSR
    mov ecx, 0xC0000080         ; EFER MSR
    rdmsr
    or eax, 0x100               ; Set LME bit (bit 8)
    wrmsr
    
    ; Enable Paging (this activates long mode)
    mov eax, cr0
    or eax, 0x80000000          ; Set PG bit (bit 31)
    mov cr0, eax
    
    ; Now we're in compatibility mode, jump to 64-bit code
    jmp 0x08:0x8000 + long_mode_64 - ap_trampoline_start

[BITS 64]
long_mode_64:
    ; Now in true 64-bit long mode
    ; Clear segments (not used in 64-bit mode except FS/GS)
    xor ax, ax
    mov ds, ax
    mov es, ax
    ; mov ss, ax
    
    ; mov rax, [0x8000+(ap_data_gdt_desc + 2 - ap_trampoline_start)]
    ; ; Now load the REAL kernel GDT
    ; mov rbx, 0xFFFFFFFF80000000
    ; add rax, rbx

    ; mov [0x8000+(ap_data_gdt_desc + 2 - ap_trampoline_start)], rax


    lgdt [0x8000 + (ap_data_gdt_desc - ap_trampoline_start)]
    
    ; ; Reload code segment with kernel's GDT
    push 0x08
    push qword (0x8000 + (.reload_cs - ap_trampoline_start))
    retfq
    
.reload_cs:
    ; Load stack pointer
    mov rsp, [0x8000 + (ap_data_stack - ap_trampoline_start)]
    add rsp, 0xFFFFFFFF80000000
    ; Verify stack is loaded correctly
    test rsp, rsp
    jz .error_no_stack
    
    and rsp, -16  ; Clear bottom 4 bits    
    xor rbp, rbp

    call enable_sse
    
    ; Signal that we're ready (set to 1)
    mov dword [0x8000 + (ap_data_ready - ap_trampoline_start)], 1
    
    ; Load entry point and jump to kernel
    mov rax, [0x8000 + (ap_data_entry - ap_trampoline_start)]
    test rax, rax
    jz .error_no_entry
    
    ; Jump to kernel entry point
    call rax
    
    ; Should never return
    jmp .hang

.error_no_stack:
    ; Stack pointer was zero
    mov dword [0x8000 + (ap_data_ready - ap_trampoline_start)], 0xDEAD0001
    jmp .hang

.error_no_entry:
    ; Entry point was zero  
    mov dword [0x8000 + (ap_data_ready - ap_trampoline_start)], 0xDEAD0002
    jmp .hang

.hang:
    cli
    hlt
    jmp .hang

; Temporary GDT for transitioning to protected/long mode
; This is a minimal flat GDT just to get into 64-bit mode
align 16

enable_sse:
    mov rax, cr0
    and rax, ~(1 << 2)      ; CR0.EM = 0
    or  rax,  (1 << 1)      ; CR0.MP = 1
    mov cr0, rax

    mov rax, cr4
    or  rax, (1 << 9) | (1 << 10) ; OSFXSR | OSXMMEXCPT
    mov cr4, rax

    fninit
    ret
temp_gdt_start:
    dq 0x0000000000000000       ; Null descriptor
    dq 0x00CF9A000000FFFF       ; Code segment (32-bit)
    dq 0x00CF92000000FFFF       ; Data segment (32-bit)
temp_gdt_end:

temp_gdt_ptr:
    dw temp_gdt_end - temp_gdt_start - 1    ; Limit
    dd 0x8000 + temp_gdt_start - ap_trampoline_start  ; Base (physical)

; Pad to offset 512 for data area
times 512 - ($ - ap_trampoline_start) db 0

; Data area - filled by BSP before starting AP
ap_data_start:

ap_data_pml4:
    dq 0                        ; uint64_t pml4_phys (offset 512)

ap_data_gdt_desc:               ; offset 520
    dw 0                        ; uint16_t gdt_limit
    dq 0                        ; uint64_t gdt_base (physical)

ap_data_stack:                  ; offset 530
    dq 0                        ; uint64_t stack_top

ap_data_entry:                  ; offset 538
    dq 0                        ; uint64_t entry_point

ap_data_ready:                  ; offset 546
    dd 0                        ; uint32_t ap_ready

ap_data_end:

ap_trampoline_end:

; Size check
%if (ap_trampoline_end - ap_trampoline_start) > 4096
    %error "Trampoline too large!"
%endif