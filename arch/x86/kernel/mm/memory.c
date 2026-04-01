#include <mm/pmm.h>
#include <mm/slab.h>
#include <mm/vmm.h>

#include <hubble/printk.h>

#include <hubble/memory.h>

void boot_memory_init()
{
	printk(KERN_INFO "Initializing Memory Management");

	printk(KERN_DEBUG "Initializing PMM...\n");
	pmm_init();
	printk(KERN_INFO "PMM initialized\n");

	printk(KERN_DEBUG "Initializing Slab Allocator...\n");
	slab_init();
	printk(KERN_INFO "Slab Allocator initialized\n");

	printk(KERN_DEBUG "Initializing VMM...\n");
	vmm_init();
	printk(KERN_INFO "VMM initialized\n");
}
