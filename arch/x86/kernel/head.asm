section .text.boot
global kernel_entry
extern kmain

kernel_entry:
    ; rdi = BootInfo* (System V ABI)
    call kmain

.hang:
    cli
    hlt
    jmp .hang
