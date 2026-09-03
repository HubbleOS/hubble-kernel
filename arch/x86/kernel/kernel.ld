KERNEL_VIRT_BASE = 0xffffffff80000000;

MEMORY
{
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

    .text : ALIGN(4096)
    {
        *(.text.boot)
        *(.text*)
        *(.text.ap_trampoline)
    } > VIRT :text

    .rodata : ALIGN(4096)
    {
        *(.rodata*)
    } > VIRT :rodata

    .limine_requests ALIGN(8) :
    {
        __limine_requests_start = .;
        KEEP(*(.limine_requests_start))
        KEEP(*(.limine_requests))
        KEEP(*(.limine_requests_end))
        __limine_requests_end = .;
    } > VIRT :rodata

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
    } > VIRT :rodata

    .data : ALIGN(4096)
    {
        *(.data*)
    } > VIRT :data

    .bss (NOLOAD) : ALIGN(4096)
    {
        _bss_start = .;
        *(.bss*)
        *(COMMON)
        _bss_end = .;
    } > VIRT :data

    . = ALIGN(4096);

    _kernel_end = .;
    _kernel_size = _kernel_end - _kernel_start;

    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note*)
    }
}
