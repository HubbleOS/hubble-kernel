OUTPUT_ARCH(aarch64)
ENTRY(_start)

SECTIONS
{
    . = 0x400000; 

    .text : {
        *(.text._start)
        *(.text*)
    }

    .rodata : { *(.rodata*) }

    .data : { *(.data*) }

    .bss : { *(.bss*) *(COMMON) }
}