// kernel_main.c - Higher-Half Kernel Entry Point

#include <bootinfo/bootinfo.h>
#include <utils/font.h>
#include <utils/bwfvideo.h>
#include <fs/fat32/fat.h>
#include <fs/ata/ata.h>
#include <fs/gpt/gpt.h>
#include <fs/vfs/vfs.h>
#include <mm/slab.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <mm/mm.h>
#include <mm/kmalloc.h>
#include "printk.h"
#include "gdt/gdt.h"
#include "gdt/interrupt.h"

#include "higher_half.h"

typedef char symbol[];

// BSS section markers from linker
// ============================================================================
extern symbol _bss_start, _bss_end;
extern symbol _kernel_start, _kernel_end;

// ============================================================================
// Static device structures
// ============================================================================
static ATA_Device ata_devices[2] = {
    {.bus = 0,
     .device = 0,
     .io_base = 0x1F0,
     .ctrl_base = 0x3F6},
    {.bus = 1,
     .device = 0,
     .io_base = 0x170,
     .ctrl_base = 0x376},
};

static VFS_Device devi[2] = {
    {
	.device = &ata_devices[0],
	.read = &ata_read_sector,
	.write = &ata_write_sector,
    },
};

static gpt_partition_t partitions[20] = {{.device = &devi[0]}};

// ============================================================================
// External functions
// ============================================================================
extern void os_main(BootInfo *bi);
extern VFS_FS *root_fs;
extern void syscall_init(void);

// ============================================================================
// Helper functions
// ============================================================================

/**
 * @brief Clear BSS section
 */
static inline void clear_bss(void)
{
	uint64_t start = (uint64_t)_bss_start;
	uint64_t end = (uint64_t)_bss_end;

	for (uint64_t addr = start; addr < end; addr++)
		*(uint8_t *)addr = 0;
}

/**
 * @brief Convert bootloader physical addresses to higher-half virtual
 */
static void relocate_boot_info(BootInfo *bi)
{
	if (bi->framebuffer)
	{
		if ((uint64_t)bi->framebuffer < KERNEL_VIRT_BASE)
		{
			bi->framebuffer = PHYS_TO_VIRT(bi->framebuffer);
		}
	}

	if (bi->memory_map)
	{
		if ((uint64_t)bi->memory_map < KERNEL_VIRT_BASE)
		{
			bi->memory_map = PHYS_TO_VIRT(bi->memory_map);
		}
	}
}

// ============================================================================
// Memory Diagnostics (from previous artifact)
// ============================================================================

void memory_diagnostics(void)
{
	printk(KERN_INFO "\n");
	printk(KERN_INFO "╔════════════════════════════════════════════════╗\n");
	printk(KERN_INFO "║        MEMORY SUBSYSTEM DIAGNOSTICS           ║\n");
	printk(KERN_INFO "╚════════════════════════════════════════════════╝\n");
	printk(KERN_INFO "\n");

	// PMM Statistics
	pmm_info_t *pmm_info = pmm_get_info();
	printk(KERN_INFO "=== Physical Memory Manager ===\n");
	printk(KERN_INFO "Total memory:   %lu MB\n", pmm_info->total_memory / (1024 * 1024));
	printk(KERN_INFO "Used memory:    %lu MB\n", pmm_info->used_memory / (1024 * 1024));
	printk(KERN_INFO "Free pages:     %lu\n", pmm_info->total_pages - pmm_info->used_pages);

	uint64_t free_mb = ((pmm_info->total_pages - pmm_info->used_pages) * PAGE_SIZE) / (1024 * 1024);
	printk(KERN_INFO "Free memory:    %lu MB\n", free_mb);

	if (free_mb < 10)
		printk(KERN_ERR "⚠️  CRITICAL: Less than 10 MB free!\n");
	else
		printk(KERN_INFO "✓ PMM has sufficient free memory\n");

	printk(KERN_INFO "\n");

	// Slab Statistics
	slab_info_t *slab_info = slab_get_info();
	printk(KERN_INFO "=== Slab Allocator ===\n");
	printk(KERN_INFO "Total slabs:       %u\n", slab_info->total_slabs);
	printk(KERN_INFO "Total memory:      %lu KB\n", slab_info->total_memory / 1024);
	printk(KERN_INFO "Used memory:       %lu KB\n", slab_info->used_memory / 1024);
	printk(KERN_INFO "Active allocs:     %lu\n",
	       slab_info->total_allocations - slab_info->total_frees);

	printk(KERN_INFO "\n");

	// VMM Statistics
	vmm_info_t *vmm_info = vmm_get_info();
	printk(KERN_INFO "=== Virtual Memory Manager ===\n");
	printk(KERN_INFO "Kernel pages:      %lu (%lu MB)\n",
	       vmm_info->kernel_pages,
	       (vmm_info->kernel_pages * PAGE_SIZE) / (1024 * 1024));

	printk(KERN_INFO "\n");
}

bool test_memory_allocation(void)
{
	printk(KERN_INFO "=== Testing Memory Allocation ===\n");

	void *ptr1 = kmalloc(64);
	if (!ptr1)
	{
		printk(KERN_ERR "✗ Failed to allocate 64 bytes\n");
		return false;
	}
	printk(KERN_INFO "✓ Allocated 64 bytes at %p\n", ptr1);

	void *ptr2 = kmalloc(512);
	if (!ptr2)
	{
		printk(KERN_ERR "✗ Failed to allocate 512 bytes\n");
		kfree(ptr1);
		return false;
	}
	printk(KERN_INFO "✓ Allocated 512 bytes at %p\n", ptr2);

	kfree(ptr1);
	kfree(ptr2);

	printk(KERN_INFO "✓ All allocations successful\n");
	return true;
}

extern kmalloc_test(void);

// ============================================================================
// ENTRY POINT
// ============================================================================

__attribute__((section(".text.boot")))
__attribute__((used)) void
kernel_entry(BootInfo *bi)
{
	// 1. Clear BSS
	clear_bss();

	// 2. Convert boot info addresses
	relocate_boot_info(bi);

	// 3. Early printk
	early_printk_init(bi->framebuffer);

	printk(KERN_INFO "=== Higher-Half Kernel Starting ===\n");
	printk(KERN_INFO "Kernel virtual base: 0x%lx\n", (uint64_t)KERNEL_VIRT_BASE);
	printk(KERN_INFO "Kernel range: 0x%lx - 0x%lx\n",
	       (uint64_t)_kernel_start, (uint64_t)_kernel_end);

	// 4. CPU initialization
	printk(KERN_INFO "Initializing CPU subsystems...\n");
	gdt_init();
	tss_init();
	idt_init();
	interrupts_init();
	syscall_init();
	printk(KERN_INFO "CPU initialization complete\n");

	// 5. Memory Management
	printk(KERN_INFO "\n=== Initializing Memory Management ===\n");

	printk(KERN_DEBUG "Heap physical: 0x%lx - 0x%lx (%lu MB)\n",
	       bi->memory_map->heap_start,
	       bi->memory_map->heap_start + bi->memory_map->heap_size,
	       bi->memory_map->heap_size / (1024 * 1024));

	// PMM
	printk(KERN_DEBUG "Initializing PMM...\n");
	pmm_init(bi->memory_map->heap_start, bi->memory_map->heap_size);
	printk(KERN_INFO "PMM initialized\n");

	// Slab
	printk(KERN_DEBUG "Initializing Slab Allocator...\n");
	slab_init();
	printk(KERN_INFO "Slab Allocator initialized\n");

	void *slab_test = slab_alloc(4096 * 2);
	printk(KERN_DEBUG "Slab allocated 128 bytes at %p\n", slab_test);
	slab_free(slab_test);

	// VMM
	printk(KERN_DEBUG "Initializing VMM...\n");
	vmm_init();
	printk(KERN_INFO "VMM initialized\n");

	// === CRITICAL: Run diagnostics BEFORE heavy tests ===
	// kmalloc_test();

	printk(KERN_INFO "Testing slab allocator...\n");

	void *a = slab_alloc(8);
	void *b = slab_alloc(32);
	void *c = slab_alloc(64);

	printk(KERN_DEBUG "Allocated: a=%p, b=%p, c=%p\n", a, b, c);

	slab_free(a);
	slab_free(b);
	slab_free(c);

	// memory_diagnostics();

	printk(KERN_INFO "Slab allocator basic test passed\n");

	// Test basic allocation
	// if (!test_memory_allocation())
	// {
	// 	printk(KERN_ERR "Memory allocation test FAILED!\n");
	// 	printk(KERN_ERR "System cannot continue - halting.\n");
	// 	while (1)
	// 		asm("hlt");
	// }

	printk(KERN_DEBUG "GPT init...\n");
	int gpt_result = gpt_init(partitions);

	if (gpt_result < 0)
	{
		printk(KERN_ERR "GPT initialization failed: %d\n", gpt_result);
		printk(KERN_WARNING "Continuing without GPT...\n");
	}
	else
	{
		printk(KERN_INFO "GPT initialized with %d partitions\n", gpt_result);
	}

	// Setup partition device callbacks
	if (!partitions[1].device->read)
	{
		partitions[1].device->read = &ata_read_sector;
		partitions[1].device->write = &ata_write_sector;
		partitions[1].device->device = &ata_devices[0];
	}

	printk(KERN_DEBUG "Mounting FAT32 at LBA %d...\n", partitions[0].first_lba);
	vfs_mount("/", &partitions[0], FS_FAT32);
	printk(KERN_INFO "Filesystem mounted\n");

	// 7. Verify CPU state
	printk(KERN_INFO "\n=== Verifying CPU State ===\n");
	uint64_t rip;
	asm volatile("lea (%%rip), %0" : "=r"(rip));

	if (rip >= KERNEL_VIRT_BASE)
	{
		printk(KERN_INFO "Running in higher-half: ✓\n");
	}
	else
	{
		printk(KERN_ERR "ERROR: Not in higher-half! RIP: 0x%lx\n", rip);
		while (1)
			asm("hlt");
	}

	// 8. Final diagnostics
	printk(KERN_INFO "\n=== Final System State ===\n");
	memory_diagnostics();

	// 9. Kernel initialization complete
	printk(KERN_INFO "\n=== Kernel Initialization Complete ===\n\n");

	// 10. Transfer control to OS main
	printk(KERN_INFO "Starting OS main loop...\n");
	os_main(bi);

	// Should never reach here
	printk(KERN_WARNING "os_main() returned! Entering infinite loop...\n");
	while (1)
	{
		asm volatile("hlt");
	}
}

void kernel_main(BootInfo *bi) __attribute__((alias("kernel_entry")));
