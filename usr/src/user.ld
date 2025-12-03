OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

SECTIONS
{
    . = 0x400000;
    
    .text : ALIGN(4096)
    {
        *(.text.start)
        *(.text .text.*)
    }
    
    .rodata : ALIGN(4096)
    {
        *(.rodata .rodata.*)
    }
    
    .data : ALIGN(4096)
    {
        *(.data .data.*)
    }
    
    .bss : ALIGN(4096)
    {
        *(COMMON)
        *(.bss .bss.*)
    }
    
    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note.GNU-stack)
        *(.note.*)
    }
}