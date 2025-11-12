/* kernel.ld - Higher-Half Kernel Linker Script */

KERNEL_VIRT_BASE = 0xffffffff80000000;
KERNEL_PHYS_BASE = 0x100000;

MEMORY
{
    /* Physical download addresses */
    PHYS (rwx) : ORIGIN = 0x100000, LENGTH = 16M
}

ENTRY(kernel_entry)

SECTIONS
{
    /* All sections use virtual addresses, but are loaded using physical addresses */
    . = KERNEL_VIRT_BASE + KERNEL_PHYS_BASE;

    /* Start of the kernel */
    _kernel_start = .;
    _kernel_phys_start = . - KERNEL_VIRT_BASE;

    /* Code */
    .text ALIGN(4K) : AT(ADDR(.text) - KERNEL_VIRT_BASE)
    {
        *(.text.boot)     /* Entry point должен быть первым */
        *(.text*)
    }

    /* Read-only data */
    .rodata ALIGN(4K) : AT(ADDR(.rodata) - KERNEL_VIRT_BASE)
    {
        *(.rodata*)
    }

    /* Initialized data */
    .data ALIGN(4K) : AT(ADDR(.data) - KERNEL_VIRT_BASE)
    {
        *(.data*)
    }

    /* 
        BSS 
    In a linker, BSS (Block Started by Symbol) 
    is the section that holds uninitialized global a
    nd static variables. 
    */
    .bss ALIGN(4K) : AT(ADDR(.bss) - KERNEL_VIRT_BASE)
    {
        _bss_start = .;
        *(.bss*)
        *(COMMON)
        _bss_end = .;
    }

    /* End of the kernel */
    . = ALIGN(4K);
    _kernel_end = .;
    _kernel_phys_end = . - KERNEL_VIRT_BASE;
    _kernel_size = _kernel_end - _kernel_start;

    /* Discarding unnecessary sections */
    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note*)
    }
}