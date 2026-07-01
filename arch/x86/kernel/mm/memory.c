/**
 * @file memory.c
 * @brief Kernel memory initialization and verification
 *
 * Coordinates the initialization of the Physical Memory Manager, Slab
 * Allocator, and Virtual Memory Manager during boot.
 */

#include <hubble/memory.h>
#include <hubble/printk.h>

#include <higher_half.h>

#include <mm/pmm.h>
#include <mm/slab.h>
#include <mm/vmm.h>

/* -- Memory Verification --------------------------------------------------- */

/**
 * @brief Check whether the kernel is running in virtual memory
 *
 * Compares a kernel virtual address with its physical translation.
 */
void check_virtual_memory(void) {
  void *addr = (void *)check_virtual_memory;
  uintptr_t phys = virt_to_phys((uint64_t)addr);

  printk(KERN_INFO "Virtual addr: %p\n", addr);
  printk(KERN_INFO "Physical addr: 0x%lx\n", phys);

  if ((uintptr_t)addr != phys)
    printk(KERN_INFO "Kernel is running in virtual memory space!\n");
  else
    printk(KERN_WARNING "Kernel still in physical memory space!\n");
  printk(KERN_INFO "Physical addr: 0x%lx\n", phys);
  printk(KERN_INFO "Virtual addr: %p\n", addr);
}

/* -- Boot Initialization --------------------------------------------------- */

/**
 * @brief Initialize all memory subsystems during boot
 *
 * Initializes PMM, slab allocator, and VMM in order.
 */
void boot_memory_init(void) {
  printk(KERN_INFO "Initializing Memory Management");

  printk(KERN_DEBUG "Initializing PMM...\n");
  pmm_init();
  printk(KERN_OK "PMM initialized\n");

  printk(KERN_DEBUG "Initializing Slab Allocator...\n");
  slab_init();
  printk(KERN_OK "Slab Allocator initialized\n");

  printk(KERN_DEBUG "Initializing VMM...\n");
  vmm_init();
  printk(KERN_OK "VMM initialized\n");

  check_virtual_memory();
}
