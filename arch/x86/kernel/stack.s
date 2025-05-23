
global kernel_entry
extern kernel_main

section .text
kernel_entry:
    mov rsp, stack_top
    call kernel_main
.loop:
    hlt
    jmp .loop

section .bss
align 16
stack_bottom:
    resb 0x4000
stack_top:
