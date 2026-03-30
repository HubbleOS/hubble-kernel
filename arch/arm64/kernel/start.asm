/* AArch64 bare-metal entry point */
.section .text
.global _start
_start:
    /* Установим стек (16 KB сверху RAM) */
    ldr x0, =stack_top
    mov sp, x0

    /* Вызов main() */
    bl main

hang:
    wfe      /* wait for event, просто застыть */
    b hang

/* стек */
.section .bss
    .align 16
stack_bottom:
    .space 16384
stack_top:
