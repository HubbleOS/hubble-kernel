    .section .text.boot
    .globl _start
    .type _start, %function

_start:
    mrs x0, mpidr_el1
    and x0, x0, #0xff
    cbz x0, 1f

0:
    wfe
    b 0b

1:
    ldr x0, =__stack_top
    mov sp, x0

    ldr x0, =__bss_start
    ldr x1, =__bss_end
2:
    cmp x0, x1
    b.hs 3f
    str xzr, [x0], #8
    b 2b

3:
    bl kernel_main

4:
    wfe
    b 4b

    .section .bss
    .align 12
__stack_bottom:
    .skip 16384
__stack_top:
