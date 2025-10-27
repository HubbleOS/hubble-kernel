MEMORY
{
    TEXT   (rx) : ORIGIN = 0x100000, LENGTH = 1M
    RODATA (rx) : ORIGIN = 0x200000, LENGTH = 64K
    DATA   (rw) : ORIGIN = 0x210000, LENGTH = 128K
    BSS    (rw) : ORIGIN = 0x230000, LENGTH = 512K  
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
