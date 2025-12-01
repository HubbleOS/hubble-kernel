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

extern int load_elf_and_run(const char *path);

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
	idt_init();
	tss_init();
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

	// VMM
	printk(KERN_DEBUG "Initializing VMM...\n");
	vmm_init();
	printk(KERN_INFO "VMM initialized\n");

	printk(KERN_INFO "Slab allocator basic test passed\n");

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

	// 8. Kernel initialization complete
	printk(KERN_INFO "\n=== Kernel Initialization Complete ===\n\n");

	// 9. Transfer control to OS main
	printk(KERN_INFO "Starting OS main loop...\n");

	load_elf_and_run("/usr/bin/user.elf");
	// char bi_char = keyboard_get_char();
	// printk(KERN_INFO "Initial key press (if any): '%c' (0x%02x)\n", bi_char ? bi_char : ' ', bi_char);
	// os_main(bi);

	// Should never reach here
	printk(KERN_WARNING "os_main() returned! Entering infinite loop...\n");
	while (1)
	{
		asm volatile("hlt");
	}
}

void kernel_main(BootInfo *bi) __attribute__((alias("kernel_entry")));
