/* kernel.ld - Higher-Half Kernel Linker Script */

KERNEL_VIRT_BASE = 0xffffffff80000000;
KERNEL_PHYS_BASE = 0x100000;

MEMORY
{
    PHYS (rwx) : ORIGIN = KERNEL_PHYS_BASE, LENGTH = 16M
}

ENTRY(kernel_entry)

SECTIONS
{
    . = KERNEL_VIRT_BASE + KERNEL_PHYS_BASE;

    /* Start of the kernel */
    _kernel_start = .;
    _kernel_phys_start = . - KERNEL_VIRT_BASE;

    /* Code */
    .text : ALIGN(4K)
    {
        *(.text.boot)     /* Entry point must be first */
        *(.text*)
        *(.text.ap_trampoline)  
    } AT > PHYS

    /* Read-only data */
    .rodata : ALIGN(4K)
    {
        *(.rodata*)
    } AT > PHYS

    /* Initcalls sections */
    /* Separating the levels initcalls */
    .initcalls ALIGN(8) :
    {
        /* Early initcalls */
        __start___initcalls_early = .;
        KEEP(*(.initcalls.early))
        __stop___initcalls_early = .;

        /* Core initcalls */
        __start___initcalls_core = .;
        KEEP(*(.initcalls.core))
        __stop___initcalls_core = .;

        /* FS initcalls */
        __start___initcalls_fs = .;
        KEEP(*(.initcalls.fs))
        __stop___initcalls_fs = .;

        /* Device initcalls */
        __start___initcalls_device = .;
        KEEP(*(.initcalls.device))
        __stop___initcalls_device = .;

        /* Late initcalls */
        __start___initcalls_late = .;
        KEEP(*(.initcalls.late))
        __stop___initcalls_late = .;

    } AT > PHYS

    /* Initialized data */
    .data : ALIGN(4K)
    {
        *(.data*)
    } AT > PHYS

    /* BSS */
    .bss : ALIGN(4K)
    {
        _bss_start = .;
        *(.bss*)
        *(COMMON)
        _bss_end = .;
    } AT > PHYS

    /* End of the kernel */
    . = ALIGN(4K);
    _kernel_end = .;
    _kernel_phys_end = . - KERNEL_VIRT_BASE;
    _kernel_size = _kernel_end - _kernel_start;

    /* Discard unnecessary sections */
    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note*)
    }
}