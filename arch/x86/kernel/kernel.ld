KERNEL_VIRT_BASE = 0xffffffff80000000;
KERNEL_PHYS_BASE = 0x100000;

MEMORY
{
    PHYS (rwx) : ORIGIN = KERNEL_PHYS_BASE, LENGTH = 16M
    VIRT (rwx) : ORIGIN = KERNEL_VIRT_BASE, LENGTH = 16M
}

PHDRS
{
    text    PT_LOAD FLAGS(5);   /* R E */
    rodata  PT_LOAD FLAGS(4);   /* R   */
    data    PT_LOAD FLAGS(6);   /* RW  */
}

ENTRY(kernel_entry)

SECTIONS
{
    . = KERNEL_VIRT_BASE;

    _kernel_start = .;
    _kernel_phys_start = ORIGIN(PHYS);

    .text : ALIGN(4096)
    {
        *(.text.boot)
        *(.text*)
        *(.text.ap_trampoline)
    } > VIRT AT > PHYS :text

    .rodata : ALIGN(4096)
    {
        *(.rodata*)
    } > VIRT AT > PHYS :rodata

    .initcalls ALIGN(8) :
    {
        __start___initcalls_early = .;
        KEEP(*(.initcalls.early))
        __stop___initcalls_early = .;

        __start___initcalls_core = .;
        KEEP(*(.initcalls.core))
        __stop___initcalls_core = .;

        __start___initcalls_fs = .;
        KEEP(*(.initcalls.fs))
        __stop___initcalls_fs = .;

        __start___initcalls_device = .;
        KEEP(*(.initcalls.device))
        __stop___initcalls_device = .;

        __start___initcalls_late = .;
        KEEP(*(.initcalls.late))
        __stop___initcalls_late = .;
    } > VIRT AT > PHYS :rodata

    .data : ALIGN(4096)
    {
        *(.data*)
    } > VIRT AT > PHYS :data

    .bss (NOLOAD) : ALIGN(4096)
    {
        _bss_start = .;
        *(.bss*)
        *(COMMON)
        _bss_end = .;
    } > VIRT AT > PHYS :data

    . = ALIGN(4096);

    _kernel_end = .;
    _kernel_size = _kernel_end - _kernel_start;

    _kernel_phys_end = LOADADDR(.text)
        + SIZEOF(.text)
        + SIZEOF(.rodata)
        + SIZEOF(.initcalls)
        + SIZEOF(.data);

    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note*)
    }
}