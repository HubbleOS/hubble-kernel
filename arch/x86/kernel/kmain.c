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

void vmm_test_basic_mapping(void)
{
	printk(KERN_INFO "=== VMM Basic Mapping Test ===\n");

	// Test 1: Map single page
	uint64_t test_phys = pmm_alloc_page();
	uint64_t test_virt = 0xFFFFFFFF90000000ULL;

	printk(KERN_DEBUG "Mapping 0x%lx -> 0x%lx\n", test_virt, test_phys);

	if (vmm_map_page(test_virt, test_phys, PTE_WRITE) == 0)
	{
		printk(KERN_INFO "✓ Page mapped successfully\n");

		// Test write/read
		uint32_t *ptr = (uint32_t *)test_virt;
		*ptr = 0xDEADBEEF;

		if (*ptr == 0xDEADBEEF)
		{
			printk(KERN_INFO "✓ Read/Write test passed\n");
		}
		else
		{
			printk(KERN_ERR "✗ Read/Write test failed\n");
		}

		// Verify physical address translation
		uint64_t phys_check = vmm_get_physical(test_virt);
		if (phys_check == test_phys)
		{
			printk(KERN_INFO "✓ Physical address translation correct\n");
		}
		else
		{
			printk(KERN_ERR "✗ Physical address mismatch: 0x%lx != 0x%lx\n",
			       phys_check, test_phys);
		}

		// Cleanup
		vmm_unmap_page(test_virt);
		pmm_free_page(test_phys);
	}
	else
	{
		printk(KERN_ERR "✗ Failed to map page\n");
		pmm_free_page(test_phys);
	}
}

/**
 * @brief Test range mapping
 */
void vmm_test_range_mapping(void)
{
	printk(KERN_INFO "\n=== VMM Range Mapping Test ===\n");

	// Allocate 16 pages (64KB)
	size_t pages = 16;
	uint64_t phys_start = pmm_alloc_pages(pages);
	uint64_t virt_start = 0xFFFFFFFF91000000ULL;

	if (phys_start == 0)
	{
		printk(KERN_ERR "✗ Failed to allocate physical pages\n");
		return;
	}

	printk(KERN_DEBUG "Mapping range: 0x%lx -> 0x%lx (%lu pages)\n",
	       virt_start, phys_start, pages);

	if (vmm_map_range(virt_start, phys_start, pages * PAGE_SIZE, PTE_WRITE) == 0)
	{
		printk(KERN_INFO "✓ Range mapped successfully\n");

		// Test write pattern across entire range
		uint32_t *ptr = (uint32_t *)virt_start;
		size_t count = (pages * PAGE_SIZE) / sizeof(uint32_t);

		for (size_t i = 0; i < count; i++)
		{
			ptr[i] = (uint32_t)i;
		}

		// Verify pattern
		bool success = true;
		for (size_t i = 0; i < count; i++)
		{
			if (ptr[i] != (uint32_t)i)
			{
				success = false;
				printk(KERN_ERR "✗ Pattern mismatch at index %lu\n", i);
				break;
			}
		}

		if (success)
		{
			printk(KERN_INFO "✓ Range write/read test passed\n");
		}

		// Cleanup
		vmm_unmap_range(virt_start, pages * PAGE_SIZE);
		pmm_free_pages(phys_start, pages);
	}
	else
	{
		printk(KERN_ERR "✗ Failed to map range\n");
		pmm_free_pages(phys_start, pages);
	}
}

/**
 * @brief Test kernel page allocation
 */
void vmm_test_kernel_allocation(void)
{
	printk(KERN_INFO "\n=== VMM Kernel Allocation Test ===\n");

	// Allocate 8 pages in kernel space
	size_t pages = 8;
	void *buffer = vmm_alloc_kernel_pages(pages);

	if (buffer)
	{
		printk(KERN_INFO "✓ Allocated %lu pages at 0x%lx\n", pages, (uint64_t)buffer);

		// Test the buffer
		uint64_t *ptr = (uint64_t *)buffer;
		size_t count = (pages * PAGE_SIZE) / sizeof(uint64_t);

		for (size_t i = 0; i < count; i++)
		{
			ptr[i] = 0xCAFEBABE00000000ULL | i;
		}

		// Verify
		bool success = true;
		for (size_t i = 0; i < count; i++)
		{
			if (ptr[i] != (0xCAFEBABE00000000ULL | i))
			{
				success = false;
				break;
			}
		}

		if (success)
		{
			printk(KERN_INFO "✓ Kernel buffer test passed\n");
		}
		else
		{
			printk(KERN_ERR "✗ Kernel buffer test failed\n");
		}

		// Free
		vmm_free_kernel_pages(buffer, pages);
		printk(KERN_INFO "✓ Kernel pages freed\n");
	}
	else
	{
		printk(KERN_ERR "✗ Failed to allocate kernel pages\n");
	}
}

/**
 * @brief Test page flag modifications
 */
void vmm_test_flags(void)
{
	printk(KERN_INFO "\n=== VMM Flags Test ===\n");

	uint64_t phys = pmm_alloc_page();
	uint64_t virt = 0xFFFFFFFF92000000ULL;

	// Map as writable
	vmm_map_page(virt, phys, PTE_WRITE);

	uint32_t *ptr = (uint32_t *)virt;
	*ptr = 0x12345678;

	printk(KERN_INFO "✓ Write to writable page: 0x%x\n", *ptr);

	// Change to read-only
	vmm_set_flags(virt, 0);
	printk(KERN_INFO "✓ Page set to read-only\n");

	// NOTE: Writing to read-only page would cause page fault
	// In real test, you'd catch the fault and verify it occurred

	// Cleanup
	vmm_unmap_page(virt);
	pmm_free_page(phys);
}

/**
 * @brief Test VMM statistics
 */
void vmm_test_statistics(void)
{
	printk(KERN_INFO "\n=== VMM Statistics ===\n");

	vmm_info_t *info = vmm_get_info();

	printk(KERN_INFO "PML4 Physical: 0x%lx\n", info->pml4_phys);
	printk(KERN_INFO "PML4 Virtual:  0x%lx\n", (uint64_t)info->pml4_virt);
	printk(KERN_INFO "Total Mapped Pages: %lu (%lu MB)\n",
	       info->total_mapped_pages,
	       (info->total_mapped_pages * PAGE_SIZE) / (1024 * 1024));
	printk(KERN_INFO "Kernel Pages: %lu (%lu MB)\n",
	       info->kernel_pages,
	       (info->kernel_pages * PAGE_SIZE) / (1024 * 1024));
}

/**
 * @brief Run all VMM tests
 */
void vmm_run_tests(void)
{
	printk(KERN_INFO "\n");
	printk(KERN_INFO "╔════════════════════════════════════╗\n");
	printk(KERN_INFO "║   VMM Test Suite                  ║\n");
	printk(KERN_INFO "╚════════════════════════════════════╝\n");
	printk(KERN_INFO "\n");

	vmm_test_basic_mapping();
	vmm_test_range_mapping();
	vmm_test_kernel_allocation();
	vmm_test_flags();
	vmm_test_statistics();

	printk(KERN_INFO "\n");
	printk(KERN_INFO "╔════════════════════════════════════╗\n");
	printk(KERN_INFO "║   All VMM Tests Complete          ║\n");
	printk(KERN_INFO "╚════════════════════════════════════╝\n");
	printk(KERN_INFO "\n");
}

/**
 * @brief Example: Map framebuffer
 */
void vmm_example_map_framebuffer(uint64_t fb_phys, size_t fb_size)
{
	printk(KERN_INFO "Mapping framebuffer: 0x%lx (%lu MB)\n",
	       fb_phys, fb_size / (1024 * 1024));

	// Framebuffer is already identity-mapped by bootloader
	// But we can remap it with specific flags if needed

	uint64_t fb_virt = PHYS_TO_VIRT(fb_phys);

	// Remap with device memory flags (uncached, write-combining)
	vmm_map_range(fb_virt, fb_phys, fb_size,
		      PTE_WRITE | PTE_NOCACHE | PTE_WRITETHROUGH);

	printk(KERN_INFO "Framebuffer mapped at: 0x%lx\n", fb_virt);
}

/**
 * @brief Example: Create heap region
 */
void *vmm_example_create_heap(size_t size_mb)
{
	size_t pages = (size_mb * 1024 * 1024) / PAGE_SIZE;

	printk(KERN_INFO "Creating %lu MB heap (%lu pages)\n", size_mb, pages);

	void *heap = vmm_alloc_kernel_pages(pages);
	if (heap)
	{
		printk(KERN_INFO "Heap created at: 0x%lx\n", (uint64_t)heap);

		// Initialize heap (zero out)
		uint8_t *ptr = (uint8_t *)heap;
		for (size_t i = 0; i < pages * PAGE_SIZE; i++)
		{
			ptr[i] = 0;
		}

		printk(KERN_INFO "Heap initialized\n");
	}

	return heap;
}

/**
 * @brief Example: Map memory-mapped device
 */
void vmm_example_map_mmio(uint64_t mmio_base, size_t mmio_size)
{
	printk(KERN_INFO "Mapping MMIO device: 0x%lx (%lu KB)\n",
	       mmio_base, mmio_size / 1024);

	uint64_t virt = PHYS_TO_VIRT(mmio_base);

	// Map with device flags (uncached, no write-combining)
	vmm_map_device(virt, mmio_base, mmio_size);

	printk(KERN_INFO "MMIO mapped at: 0x%lx\n", virt);

	// Can now access device through virt address
	volatile uint32_t *device = (volatile uint32_t *)virt;
	uint32_t status = device[0]; // Read device status register

	printk(KERN_DEBUG "Device status: 0x%x\n", status);
}

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
	vmm_init();
	printk(KERN_INFO "VMM initialized\n");

	vmm_run_tests();

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
