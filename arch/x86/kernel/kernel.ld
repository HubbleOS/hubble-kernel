/* kernel.ld - Higher-Half Kernel Linker Script */

KERNEL_VIRT_BASE = 0xffffffff80000000;
KERNEL_PHYS_BASE = 0x100000;

MEMORY
{
    /* Physical download addresses */
    PHYS (rwx) : ORIGIN = KERNEL_PHYS_BASE, LENGTH = 16M
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

    /* Section for initcalls */
    .initcalls ALIGN(8) : {
        __initcalls_start = .;
        KEEP(*(.initcalls))
        __initcalls_end = .;
    } AT > PHYS



    /* Initialized data */
    .data : ALIGN(4K)
    {
        *(.data*)
    } AT > PHYS

    /* 
        BSS 
    In a linker, BSS (Block Started by Symbol) is the section 
    that holds uninitialized global and static variables. 
    */
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

    /* Discarding unnecessary sections */
    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note*)
    }
}