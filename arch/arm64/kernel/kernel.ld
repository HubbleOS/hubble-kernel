ENTRY(_start)

PHDRS
{
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
}

SECTIONS
{
    . = 0x80000;

    __kernel_start = .;

    .text : {
        KEEP(*(.text.boot))
        *(.text*)
    } :text

    .rodata : ALIGN(8) {
        *(.rodata*)
    } :text

    . = ALIGN(0x1000);
    .data : ALIGN(8) {
        *(.data*)
    } :data

    .bss (NOLOAD) : ALIGN(16) {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    } :data

    . = ALIGN(16);
    __kernel_end = .;
}
