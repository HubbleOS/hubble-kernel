#include "init.h"

#include <mm/pmm.h>
#include <mm/slab.h>
#include <mm/vmm.h>

#include <printk.h>

void init_memory(BootInfo *bi)
{
	printk(KERN_INFO "\n=== Initializing Memory Management ===\n");

	printk(KERN_DEBUG "Heap physical: 0x%lx - 0x%lx (%lu MB)\n",
	       bi->memory_map->heap_start,
	       bi->memory_map->heap_start + bi->memory_map->heap_size,
	       bi->memory_map->heap_size / (1024 * 1024));

	printk(KERN_DEBUG "Initializing PMM...\n");
	pmm_init(bi->memory_map->heap_start, bi->memory_map->heap_size);
	printk(KERN_INFO "PMM initialized\n");

	printk(KERN_DEBUG "Initializing Slab Allocator...\n");
	slab_init();
	printk(KERN_INFO "Slab Allocator initialized\n");

	printk(KERN_DEBUG "Initializing VMM...\n");
	vmm_init();
	printk(KERN_INFO "VMM initialized\n");
}
