ENTRY(_start)

MEMORY {
    RAM (rwx) : ORIGIN = 0x400000, LENGTH = 16M
}

SECTIONS
{
    . = ORIGIN(RAM);

    .text : ALIGN(4096)
    {
        *(.text.start)
        *(.text .text.*)
    } > RAM

    .rodata : ALIGN(4096)
    {
        *(.rodata .rodata.*)
    } > RAM

    .data : ALIGN(4096)
    {
        *(.data .data.*)
    } > RAM

    .bss : ALIGN(4096)
    {
        *(COMMON)
        *(.bss .bss.*)
    } > RAM

    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note.GNU-stack)
        *(.note.*)
    }
}
