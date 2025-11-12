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
 *
 * BSS должен быть очищен перед использованием глобальных переменных
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
 *
 * @param bi Boot info structure with physical addresses
 */
static void relocate_boot_info(BootInfo *bi)
{
	// Framebuffer base остается физическим (identity mapped 0-4GB)
	// Но структуры boot info нужно конвертировать

	if (bi->framebuffer)
	{
		// Проверяем, если адрес еще не в higher-half
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
// ENTRY POINT - MUST BE IN .text.boot SECTION
// ============================================================================

/**
 * @brief Main kernel entry point
 *
 * Вызывается из загрузчика с виртуальным адресом 0xFFFFFFFF80100000
 * Stack уже настроен загрузчиком
 *
 * КРИТИЧНО: Эта функция должна быть первой в .text секции!
 */
__attribute__((section(".text.boot")))
__attribute__((used)) void
kernel_entry(BootInfo *bi)
{
	// ========================================================================
	// 1. Clear BSS first (критично для глобальных переменных!)
	// ========================================================================
	clear_bss();

	// ========================================================================
	// 2. Convert boot info addresses to higher-half
	// ========================================================================
	relocate_boot_info(bi);

	// ========================================================================
	// 3. Ранняя инициализация вывода
	// ========================================================================
	early_printk_init(bi->framebuffer);

	printk(KERN_INFO "=== Higher-Half Kernel Starting ===\n");
	printk(KERN_INFO "Kernel virtual base: 0x%lx\n", (uint64_t)KERNEL_VIRT_BASE);
	printk(KERN_INFO "Kernel range: 0x%lx - 0x%lx\n",
	       (uint64_t)_kernel_start, (uint64_t)_kernel_end);

	// ========================================================================
	// 4. CPU инициализация: GDT, IDT, Interrupts, Syscalls
	// ========================================================================
	printk(KERN_INFO "Initializing CPU subsystems...\n");

	printk(KERN_DEBUG "  GDT init\n");
	gdt_init();

	printk(KERN_DEBUG "  TSS init\n");
	tss_init();

	printk(KERN_DEBUG "  IDT init\n");
	idt_init();

	printk(KERN_DEBUG "  Interrupts init\n");
	interrupts_init();

	printk(KERN_DEBUG "  Syscalls init\n");
	syscall_init();

	printk(KERN_INFO "CPU initialization complete\n");

	// ========================================================================
	// 5. Memory Management
	// ========================================================================
	printk(KERN_INFO "\n=== Initializing Memory Management ===\n");

	printk(KERN_DEBUG "Heap physical: 0x%lx - 0x%lx (%lu MB)\n",
	       bi->memory_map->heap_start,
	       bi->memory_map->heap_start + bi->memory_map->heap_size,
	       bi->memory_map->heap_size / (1024 * 1024));

	// PMM работает с физическими адресами
	printk(KERN_DEBUG "Initializing PMM...\n");
	pmm_init(bi->memory_map->heap_start, bi->memory_map->heap_size);
	printk(KERN_INFO "PMM initialized\n");

	// Slab allocator для kernel heap
	printk(KERN_DEBUG "Initializing Slab Allocator...\n");
	slab_init();
	printk(KERN_INFO "Slab Allocator initialized\n");

	// VMM теперь использует higher-half адреса
	printk(KERN_DEBUG "Initializing VMM (higher-half mode)...\n");
	vmm_init(bi->memory_map->pml4_phys);
	printk(KERN_INFO "VMM initialized\n");

	// ========================================================================
	// 6. Storage subsystems
	// ========================================================================
	printk(KERN_INFO "\n=== Initializing Storage ===\n");

	printk(KERN_DEBUG "GPT init...\n");
	gpt_init(partitions);

	// Setup partition device callbacks if needed
	if (!partitions[1].device->read)
	{
		partitions[1].device->read = &ata_read_sector;
		partitions[1].device->write = &ata_write_sector;
		partitions[1].device->device = &ata_devices[0];
	}

	printk(KERN_DEBUG "Mounting FAT32 at LBA %d...\n", partitions[0].first_lba);
	vfs_mount("/", &partitions[0], FS_FAT32);
	printk(KERN_INFO "Filesystem mounted\n");

	// ========================================================================
	// 7. Verify CPU state
	// ========================================================================
	printk(KERN_INFO "\n=== Verifying CPU State ===\n");
	uint16_t cs, ds;
	uint64_t cr3, rip;
	asm volatile("mov %%cs, %0" : "=r"(cs));
	asm volatile("mov %%ds, %0" : "=r"(ds));
	asm volatile("mov %%cr3, %0" : "=r"(cr3));
	asm volatile("lea (%%rip), %0" : "=r"(rip));

	printk(KERN_DEBUG "CS: 0x%x, DS: 0x%x\n", cs, ds);
	printk(KERN_DEBUG "CR3 (page tables): 0x%lx\n", cr3);
	printk(KERN_DEBUG "RIP: 0x%lx\n", rip);

	// Verify we're running in higher-half
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

	// ========================================================================
	// 8. Kernel initialization complete
	// ========================================================================
	printk(KERN_INFO "\n=== Kernel Initialization Complete ===\n\n");

	// ========================================================================
	// 9. Transfer control to OS main
	// ========================================================================
	printk(KERN_INFO "Starting OS main loop...\n");
	os_main(bi);

	// ========================================================================
	// 10. Should never reach here
	// ========================================================================
	printk(KERN_WARNING "os_main() returned! Entering infinite loop...\n");
	while (1)
	{
		asm volatile("hlt");
	}
}

// ============================================================================
// Compatibility wrapper for old kernel_main name
// ============================================================================
void kernel_main(BootInfo *bi) __attribute__((alias("kernel_entry")));

// ============================================================================
// Helper для конвертации физ. адреса в виртуальный
// ============================================================================
void *phys_to_virt(uint64_t phys_addr)
{
	return (void *)(phys_addr + KERNEL_VIRT_BASE);
}

// ============================================================================
// Helper для конвертации виртуального адреса в физ.
// ============================================================================
uint64_t virt_to_phys(void *virt_addr)
{
	uint64_t addr = (uint64_t)virt_addr;
	if (addr >= KERNEL_VIRT_BASE)
	{
		return addr - KERNEL_VIRT_BASE;
	}
	return addr; // Already physical (shouldn't happen in kernel space)
}
