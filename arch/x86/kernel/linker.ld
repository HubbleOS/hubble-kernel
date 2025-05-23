MEMORY
{
    TEXT   (rx) : ORIGIN = 0x100000, LENGTH = 1M
    RODATA (rx) : ORIGIN = 0x110000, LENGTH = 64K
    DATA   (rw) : ORIGIN = 0x120000, LENGTH = 64K
    BSS    (rw) : ORIGIN = 0x130000, LENGTH = 64K
}

ENTRY(kernel_entry)

SECTIONS
{
    .text : {
        *(.text*)
    } > TEXT

    .rodata : {
        *(.rodata*)
    } > RODATA

    .data : {
        *(.data*)
    } > DATA

    .bss : {
        *(.bss*)
        *(COMMON)
    } > BSS
}
