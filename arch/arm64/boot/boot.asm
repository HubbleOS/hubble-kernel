// AArch64 Boot
// Entry point for the ARM64 kernel. Handles multi-core
// initialization, stack setup, BSS clearing, and jumps to
// the C kernel_main function.

// To keep this in the first portion of the binary.
.section ".text.boot"

// Make _start global.
.globl _start

// Entry point for the kernel. Registers:
// x0 -> 32 bit pointer to DTB in memory (primary core only) / 0 (secondary cores)
// x1 -> 0
// x2 -> 0
// x3 -> 0
// x4 -> 32 bit kernel entry point, _start location
_start:
    mrs     x0, mpidr_el1
    and     x0, x0, #0xFF
    cbnz    x0, halt

    // Set stack before our code
    ldr     x5, =_start
    mov     sp, x5

    // Clear BSS
    ldr     x5, =__bss_start
    ldr     w6, =__bss_size
1:  cbz     w6, 2f
    str     xzr, [x5], #8
    sub     w6, w6, #1
    cbnz    w6, 1b

    // Jump to C code, should not return
2:  bl      kernel_main
    // For failsafe, halt this core
halt:
    wfe
    b       halt
