; -- Kernel Entry Jump -----------------------------------------
; Jumps to the kernel entry point with a clean register state.
; Arguments (ms_abi): rcx = boot_info, rdx = entry, r8 = stack
; --------------------------------------------------------------

section .text

global jump_to_kernel
jump_to_kernel:
    mov     rsp, r8             ; switch stack
    mov     rdi, rcx            ; save boot_info in rdi
    mov     r11, rdx            ; save entry
    xor     rbp, rbp
    xor     rax, rax
    xor     rbx, rbx
    xor     rcx, rcx
    xor     rdx, rdx
    xor     rsi, rsi
    jmp     r11                 ; jump to kernel
