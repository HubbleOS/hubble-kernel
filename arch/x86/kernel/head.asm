; -- Kernel Head -----------------------------------------------
; Initial entry point for the kernel. Saves the boot info
; pointer and calls the C start_kernel function.
; --------------------------------------------------------------

section .text.boot

extern start_kernel
extern g_boot_info
extern _bss_start
extern _bss_end

global kernel_entry
kernel_entry:
    mov     qword [rel g_boot_info], rdi
    call    start_kernel

.hang:
    cli
    hlt
    jmp     .hang
