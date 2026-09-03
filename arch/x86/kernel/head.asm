; -- Kernel Head -----------------------------------------------
; Initial entry point for the kernel under Limine boot protocol.
;
; Limine provides:
;   - 4-level page tables with kernel mapped at KERNEL_VIRT_BASE
;   - HHDM mapping of physical memory
;   - A 64KiB+ stack (rsp points to top)
;   - All GPRs zeroed except rsp
;   - PG, WP, PE, PAE, LME, LMA, NX all enabled
;
; The kernel consumes Limine responses directly from the
; .limine_requests section — no boot-info pointer is passed.
; --------------------------------------------------------------

section .text.boot

extern start_kernel
extern _bss_start
extern _bss_end

global kernel_entry
kernel_entry:
    ; Zero BSS
    lea     rdi, [rel _bss_start]
    lea     rcx, [rel _bss_end]
    cmp     rdi, rcx
    jge     .bss_done
    xor     rax, rax
.bss_loop:
    mov     qword [rdi], rax
    add     rdi, 8
    cmp     rdi, rcx
    jl      .bss_loop
.bss_done:

    ; Jump to kernel main — no arguments needed.
    ; All Limine data is accessed via request structures in memory.
    call    start_kernel

.hang:
    cli
    hlt
    jmp     .hang
