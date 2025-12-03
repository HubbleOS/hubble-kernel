#include "init.h"

#include <printk.h>

// Додайте це до init_memory() перед vmm_init()

static inline void check_nx_support(void)
{
	uint32_t eax, ebx, ecx, edx;

	// CPUID 0x80000001 - Extended features
	__asm__ volatile("cpuid"
			 : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
			 : "a"(0x80000001));

	bool nx_supported = (edx & (1 << 20)) != 0;
	printk(KERN_INFO "NX bit support: %s\n", nx_supported ? "YES" : "NO");

	if (nx_supported)
	{
		// Перевірка, чи EFER.NXE увімкнено
		uint32_t efer_lo, efer_hi;
		__asm__ volatile("rdmsr" : "=a"(efer_lo), "=d"(efer_hi) : "c"(0xC0000080));
		bool nx_enabled = (efer_lo & (1 << 11)) != 0;
		printk(KERN_INFO "EFER.NXE enabled: %s\n", nx_enabled ? "YES" : "NO");

		if (!nx_enabled)
		{
			printk(KERN_WARNING "Enabling EFER.NXE...\n");
			efer_lo |= (1 << 11);
			__asm__ volatile("wrmsr" ::"a"(efer_lo), "d"(efer_hi), "c"(0xC0000080));
			printk(KERN_INFO "EFER.NXE enabled!\n");
		}
	}
}

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

	check_nx_support();

	printk(KERN_DEBUG "Initializing VMM...\n");
	vmm_init();
	printk(KERN_INFO "VMM initialized\n");
}
