section .text
global _start
extern efi_main

_start:
    sub rsp, 40
    call efi_main
    add rsp, 40
.hang:
    hlt
    jmp .hang

; void jump_to_kernel(void *boot_info, void *entry, uint64_t stack)
; ms_abi: rcx = boot_info, rdx = entry, r8 = stack
global jump_to_kernel
jump_to_kernel:
    mov rsp, r8     ; switch stack
    mov rdi, rcx    ; save boot_info in rdi before overwriting rcx
    mov r11, rdx    ; save entry
    xor rbp, rbp
    xor rax, rax
    xor rbx, rbx
    xor rcx, rcx
    xor rdx, rdx
    xor rsi, rsi
    jmp r11         ; jump to kernel