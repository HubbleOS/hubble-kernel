#include <bootinfo/bootinfo.h>
#include <utils/font.h>
#include <utils/bwfvideo.h>

#include <fs/fat32/fat.h>
#include <fs/fat32/fat_structs.h>
#include <fs/ata/ata.h>
#include <fs/gpt/gpt.h>
#include <fs/gpt/gpt_struct.h>
#include <fs/vfs/vfs.h>
#include <fs/vfs/vfs_standart_struct.h>
#include <fs/nvme/nvme.h>
#include <fs/pci/pci.h>

#include <fs/ext2/ext2.h>

#include <mm/slab.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <mm/mm.h>

#include "printk.h"

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
// static struct nvme_controller nvme_devices[1] = {
//     {
// 	.admin_cq = NULL,
// 	.admin_cq_head = 0,
// 	.admin_sq = NULL,
// 	.admin_sq_tail = 0,
// 	.bar = (volatile uint8_t *)0x100000, // фіксована адреса для QEMU
// 	.phase = 1,
//     }};

static VFS_Device devi[2] = {
    {
	.device = &ata_devices[0],
	.read = &ata_read_sector,
	.write = &ata_write_sector,

    },
    //      {
    // 	 .device = &nvme_devices[0],
    // 	 .read = &vfs_nvme_read,
    // 	 .write = &vfs_nvme_write,
};

static gpt_partition_t partitions[20] = {{.device = &devi[0]}};

extern void os_main(BootInfo *bi);
extern VFS_FS *root_fs;
BootInfo boot_info;

extern void syscall_init(void);

#include "gdt/gdt.h"
#include "gdt/interrupt.h"

extern void keyboard_handler(registers_t *regs);

// void test_slab_allocator(void);

// // Приклад структур для виділення
typedef struct task
{
	uint64_t id;
	uint64_t state;
	uint64_t stack_pointer;
	uint64_t page_table;
	char name[32];
} task_t;

typedef struct file_descriptor
{
	uint64_t inode;
	uint64_t offset;
	uint32_t flags;
	uint32_t refcount;
} fd_t;

void kernel_main(BootInfo *bi)
{
	// ========================================================================
	// Ранняя инициализация вывода
	// ========================================================================
	early_printk_init(bi->framebuffer);
	printk(KERN_INFO "Kernel starting...\n");

	// ========================================================================
	// CPU инициализация: GDT, IDT, Interrupts, Syscalls
	// ========================================================================
	printk(KERN_INFO "GDT init\n");
	gdt_init();

	printk(KERN_INFO "TSS init\n");
	tss_init();

	printk(KERN_INFO "IDT init\n");
	idt_init();

	printk(KERN_INFO "Interrupts init\n");
	interrupts_init();

	printk(KERN_INFO "Syscalls init\n");
	syscall_init();

	printk(KERN_INFO "=== Memory Initialization ===\n");

	ram_info_t *ram_info = bi->memory_map;

	printk("Kernel started!\n");
	printk("Heap start: 0x%llx\n", ram_info->heap_start);
	printk("Heap size: %llu MB\n", ram_info->heap_size / (1024 * 1024));

	// Ініціалізуємо PMM
	printk("\n=== Initializing PMM ===\n");
	pmm_init(ram_info->heap_start, ram_info->heap_size);
	printk("PMM initialized\n");

	// Виводимо статистику
	pmm_info_t *pmm_info = pmm_get_info();
	printk("PMM initialized successfully!\n");
	printk("  Total pages: %llu\n", pmm_info->total_pages);
	printk("  Used pages: %llu\n", pmm_info->used_pages);
	printk("  Free pages: %llu\n", pmm_info->total_pages - pmm_info->used_pages);
	printk("  Bitmap size: %llu KB\n", pmm_info->bitmap_size / 1024);

	// === Приклад використання PMM ===

	// Виділяємо одну сторінку
	printk("\n--- Testing single page allocation ---\n");
	uint64_t page1 = pmm_alloc_page();
	if (page1)
	{
		printk("Allocated page at: 0x%llx\n", page1);
	}
	else
	{
		printk("Failed to allocate page!\n");
	}

	// Виділяємо ще одну сторінку
	uint64_t page2 = pmm_alloc_page();
	if (page2)
	{
		printk("Allocated page at: 0x%llx\n", page2);
	}

	// Виділяємо 10 послідовних сторінок
	printk("\n--- Testing multiple page allocation ---\n");
	uint64_t pages = pmm_alloc_pages(10);
	if (pages)
	{
		printk("Allocated 10 pages starting at: 0x%llx\n", pages);
	}
	else
	{
		printk("Failed to allocate 10 pages!\n");
	}

	// Показуємо статистику після виділення
	printk("\nAfter allocations:\n");
	printk("  Used pages: %llu\n", pmm_info->used_pages);
	printk("  Free pages: %llu\n", pmm_info->total_pages - pmm_info->used_pages);

	// Звільняємо першу сторінку
	printk("\n--- Testing page deallocation ---\n");
	pmm_free_page(page1);
	printk("Freed page at: 0x%llx\n", page1);

	// Звільняємо 10 сторінок
	pmm_free_pages(pages, 10);
	printk("Freed 10 pages starting at: 0x%llx\n", pages);

	// Показуємо статистику після звільнення
	printk("\nAfter deallocations:\n");
	printk("  Used pages: %llu\n", pmm_info->used_pages);
	printk("  Free pages: %llu\n", pmm_info->total_pages - pmm_info->used_pages);

	// Тепер можеш використовувати PMM для інших компонентів ядра
	printk("\nPMM is ready for use!\n");

	// Приклад: виділення пам'яті для page table
	printk("\n--- Example: Allocating page table ---\n");
	uint64_t page_table = pmm_alloc_page();
	if (page_table)
	{
		printk("Page table allocated at: 0x%llx\n", page_table);
		// Тут можна ініціалізувати page table
		// uint64_t *pt = (uint64_t *)page_table;
		// memset(pt, 0, PAGE_SIZE);
	}

	// Ініціалізуємо Slab Allocator
	printk("\n=== Initializing Slab Allocator ===\n");
	slab_init();
	printk("Slab allocator initialized\n");

	// Запускаємо тести
	// test_slab_allocator();

	printk("\n=== Testing Slab Allocator ===\n");

	// === 1. Базове використання kmalloc/kfree ===
	printk("\n--- Test 1: Basic kmalloc/kfree ---\n");

	void *ptr1 = kmalloc(32);
	printk("kmalloc(32) = 0x%llx\n", (uint64_t)ptr1);

	void *ptr2 = kmalloc(100);
	printk("kmalloc(100) = 0x%llx\n", (uint64_t)ptr2);

	void *ptr3 = kmalloc(1024);
	printk("kmalloc(1024) = 0x%llx\n", (uint64_t)ptr3);

	kfree(ptr1);
	kfree(ptr2);
	kfree(ptr3);
	printk("All freed successfully\n");

	// === 2. Створення custom cache для структур ===
	printk("\n--- Test 2: Custom cache for task_t ---\n");

	slab_cache_t *task_cache = slab_cache_create("task_cache",
						     sizeof(task_t),
						     64);
	if (task_cache)
	{
		printk("Task cache created: object_size=%zu\n", task_cache->object_size);

		// Виділяємо кілька task'ів
		task_t *task1 = (task_t *)slab_alloc(task_cache);
		task_t *task2 = (task_t *)slab_alloc(task_cache);
		task_t *task3 = (task_t *)slab_alloc(task_cache);

		if (task1 && task2 && task3)
		{
			printk("Allocated 3 tasks:\n");
			printk("  task1 = 0x%llx\n", (uint64_t)task1);
			printk("  task2 = 0x%llx\n", (uint64_t)task2);
			printk("  task3 = 0x%llx\n", (uint64_t)task3);

			// Ініціалізуємо task'и
			task1->id = 1;
			task1->state = 1;
			strcpy(task1->name, "init");

			task2->id = 2;
			task2->state = 1;
			strcpy(task2->name, "kernel_thread");

			// Звільняємо один task
			slab_free(task_cache, task2);
			printk("Freed task2\n");

			// Виділяємо знову - повинен повернути той самий адрес
			task_t *task4 = (task_t *)slab_alloc(task_cache);
			printk("task4 = 0x%llx (should reuse task2's memory)\n",
			       (uint64_t)task4);

			slab_free(task_cache, task1);
			slab_free(task_cache, task3);
			slab_free(task_cache, task4);
		}
	}

	// === 3. Тест великих виділень ===
	printk("\n--- Test 3: Large allocations ---\n");

	void *large1 = kmalloc(8192); // 2 pages
	printk("kmalloc(8192) = 0x%llx\n", (uint64_t)large1);

	void *large2 = kmalloc(1024 * 1024); // 256 pages (1MB)
	printk("kmalloc(1MB) = 0x%llx\n", (uint64_t)large2);

	kfree(large1);
	kfree(large2);
	printk("Large allocations freed\n");

	// === 4. Стрес-тест ===
	printk("\n--- Test 4: Stress test ---\n");

#define NUM_ALLOCS 100
	void *ptrs[NUM_ALLOCS];

	printk("Allocating %d objects...\n", NUM_ALLOCS);
	for (int i = 0; i < NUM_ALLOCS; i++)
	{
		ptrs[i] = kmalloc(64);
		if (!ptrs[i])
		{
			printk("Allocation %d failed!\n", i);
			break;
		}
	}

	printk("Freeing all objects...\n");
	for (int i = 0; i < NUM_ALLOCS; i++)
	{
		if (ptrs[i])
		{
			kfree(ptrs[i]);
		}
	}
	printk("Stress test completed\n");

	// === 5. Вирівняне виділення ===
	printk("\n--- Test 5: Aligned allocation ---\n");

	void *aligned = kmalloc_aligned(256, 256);
	printk("kmalloc_aligned(256, 256) = 0x%llx\n", (uint64_t)aligned);

	if ((uint64_t)aligned % 256 == 0)
	{
		printk("Alignment verified!\n");
	}
	else
	{
		printk("ERROR: Alignment failed!\n");
	}

	kfree(aligned);

	// === 6. Статистика ===
	printk("\n--- Final Statistics ---\n");
	slab_print_stats();

	printk("\nPMM Statistics:\n");
	printk("  Used memory: %llu MB\n", pmm_info->used_memory / (1024 * 1024));
	printk("  Free memory: %llu MB\n",
	       (pmm_info->total_memory - pmm_info->used_memory) / (1024 * 1024));

	printk("\n=== Initializing VMM... ===\n");
	vmm_init();
	printk("VMM initialized\n");

	printk("\n=== Testing VMM ===\n");

	// === 1. Базове мапування сторінки ===
	printk("\n--- Test 1: Basic page mapping ---\n");

	uint64_t test_virt = 0xFFFF800000001000ULL;
	uint64_t test_phys = pmm_alloc_page();

	if (test_phys)
	{
		printk("Allocated physical page: 0x%llx\n", test_phys);

		if (vmm_map_page(test_virt, test_phys, PAGE_KERNEL))
		{
			printk("Mapped 0x%llx -> 0x%llx\n", test_virt, test_phys);

			// Перевіряємо мапінг
			uint64_t phys_check = vmm_virt_to_phys(test_virt);
			printk("Verification: virt_to_phys(0x%llx) = 0x%llx\n",
			       test_virt, phys_check);

			if (phys_check == test_phys)
			{
				printk("✓ Mapping verified!\n");
			}
			else
			{
				printk("✗ Mapping verification failed!\n");
			}

			// Тестуємо запис/читання
			uint64_t *ptr = (uint64_t *)test_virt;
			*ptr = 0xDEADBEEFCAFEBABEULL;

			if (*ptr == 0xDEADBEEFCAFEBABEULL)
			{
				printk("✓ Write/read test passed!\n");
			}
			else
			{
				printk("✗ Write/read test failed!\n");
			}

			// Анмапимо
			vmm_unmap_page(test_virt);
			printk("Unmapped page\n");
		}
	}

	// === 2. Мапування діапазону ===
	printk("\n--- Test 2: Range mapping ---\n");

	uint64_t range_virt = 0xFFFF800000010000ULL;
	uint64_t range_phys = pmm_alloc_pages(4);

	if (range_phys)
	{
		printk("Allocated 4 physical pages starting at: 0x%llx\n", range_phys);

		if (vmm_map_range(range_virt, range_phys, 4 * PAGE_SIZE_4K, PAGE_KERNEL))
		{
			printk("Mapped range: 0x%llx -> 0x%llx (16KB)\n",
			       range_virt, range_phys);

			// Перевіряємо кожну сторінку
			bool all_mapped = true;
			for (int i = 0; i < 4; i++)
			{
				uint64_t virt = range_virt + i * PAGE_SIZE_4K;
				uint64_t phys = vmm_virt_to_phys(virt);
				uint64_t expected_phys = range_phys + i * PAGE_SIZE_4K;

				if (phys != expected_phys)
				{
					printk("✗ Page %d: expected 0x%llx, got 0x%llx\n",
					       i, expected_phys, phys);
					all_mapped = false;
				}
			}

			if (all_mapped)
			{
				printk("✓ All pages mapped correctly!\n");
			}

			// Тестуємо запис через весь діапазон
			uint64_t *ptr = (uint64_t *)range_virt;
			for (int i = 0; i < 512 * 4; i++)
			{ // 4 pages * 512 entries
				ptr[i] = i;
			}

			bool write_ok = true;
			for (int i = 0; i < 512 * 4; i++)
			{
				if (ptr[i] != i)
				{
					write_ok = false;
					break;
				}
			}

			if (write_ok)
			{
				printk("✓ Range write/read test passed!\n");
			}
			else
			{
				printk("✗ Range write/read test failed!\n");
			}

			vmm_unmap_range(range_virt, 4 * PAGE_SIZE_4K);
			printk("Unmapped range\n");
		}
	}

	// === 3. vmm_alloc / vmm_free ===
	printk("\n--- Test 3: vmm_alloc/vmm_free ---\n");

	uint64_t alloc_virt = vmm_alloc(8192, PAGE_KERNEL); // 2 pages
	if (alloc_virt)
	{
		printk("vmm_alloc(8192) = 0x%llx\n", alloc_virt);

		// Записуємо щось
		uint64_t *ptr = (uint64_t *)alloc_virt;
		ptr[0] = 0x1122334455667788ULL;

		if (ptr[0] == 0x1122334455667788ULL)
		{
			printk("✓ vmm_alloc memory is usable!\n");
		}

		vmm_free(alloc_virt, 8192);
		printk("Freed allocated memory\n");
	}

	// === 4. Зміна флагів ===
	printk("\n--- Test 4: Changing page flags ---\n");

	uint64_t flags_virt = 0xFFFF800000020000ULL;
	uint64_t flags_phys = pmm_alloc_page();

	if (flags_phys)
	{
		// Мапимо як read-only
		vmm_map_page(flags_virt, flags_phys, PAGE_READONLY);

		uint64_t flags = vmm_get_flags(flags_virt);
		printk("Initial flags: 0x%llx (should be read-only)\n", flags);

		// Змінюємо на read-write
		vmm_set_flags(flags_virt, PAGE_KERNEL);

		flags = vmm_get_flags(flags_virt);
		printk("Updated flags: 0x%llx (should be read-write)\n", flags);

		if (flags & PAGE_WRITE)
		{
			printk("✓ Flags changed successfully!\n");
		}

		vmm_unmap_page(flags_virt);
	}

	// === 5. Створення нового адресного простору ===
	printk("\n--- Test 5: Creating new address space ---\n");

	address_space_t *new_as = vmm_create_address_space();
	if (new_as)
	{
		printk("Created new address space:\n");
		printk("  PML4 physical: 0x%llx\n", new_as->pml4_phys);
		printk("  Heap start: 0x%llx\n", new_as->heap_start);
		printk("  Stack start: 0x%llx\n", new_as->stack_start);

		// Зберігаємо поточний address space
		address_space_t *old_as = vmm_get_current_address_space();
		printk("Current address space PML4: 0x%llx\n", old_as->pml4_phys);

		// Перемикаємось на новий address space
		vmm_switch_address_space(new_as);
		printk("Switched to new address space\n");

		// Перевіряємо що CR3 змінився
		uint64_t cr3 = vmm_get_cr3();
		if (cr3 == new_as->pml4_phys)
		{
			printk("✓ Address space switch successful!\n");
		}

		// Повертаємось назад
		vmm_switch_address_space(old_as);
		printk("Switched back to original address space\n");

		// Видаляємо новий address space
		vmm_destroy_address_space(new_as);
		printk("Destroyed new address space\n");
	}

	// === 6. Статистика ===
	printk("\n--- Final Statistics ---\n");
	vmm_print_stats();

	printk("\n=== All tests completed ===\n");

	printk("\n=== Real-world example: Creating kernel heap ===\n");

	// Виділяємо 1MB для kernel heap
	uint64_t heap_virt = vmm_alloc(1024 * 1024, PAGE_KERNEL);
	if (heap_virt)
	{
		printk("Kernel heap allocated at: 0x%llx (1MB)\n", heap_virt);

		// Тепер можна використовувати цю пам'ять для будь-чого
		// Наприклад, як backing memory для heap allocator

		uint64_t *test_data = (uint64_t *)heap_virt;
		test_data[0] = 0xCAFEBABE;
		test_data[1] = 0xDEADBEEF;

		printk("Written test data to heap\n");
		printk("  [0] = 0x%llx\n", test_data[0]);
		printk("  [1] = 0x%llx\n", test_data[1]);
	}

	// ========================================================================
	// Остальная инициализация
	// ========================================================================
	// printk(KERN_INFO "GPT init\n");
	// gpt_init(partitions);

	// printk(KERN_INFO "FAT32 init\n");
	// vfs_mount("/", &partitions[0], FS_FAT32);

	// printk(KERN_INFO "Kernel initialization complete\n");

	// // ========================================================================
	// // Тесты прерываний и системных вызовов
	// // ========================================================================
	// printk(KERN_INFO "=== Interrupt Tests ===\n");

	// uint16_t cs, ds;
	// asm volatile("mov %%cs, %0" : "=r"(cs));
	// asm volatile("mov %%ds, %0" : "=r"(ds));
	// printk("CS: 0x%x, DS: 0x%x\n", cs, ds);

	// printk(KERN_INFO "Test syscall(0) - expecting 666\n");
	// uint64_t result;

	// asm volatile(
	//     "movq $0, %%rax\n"
	//     "int $0x80\n"
	//     "movq %%rax, %0"
	//     : "=r"(result)
	//     :
	//     : "rax");

	// printk(KERN_INFO "Result: %lu (0x%lx)\n", result, result);

	// ========================================================================
	// Переход в основной код ядра
	// ========================================================================
	// os_main(bi);

	printk(KERN_INFO "Entering main loop\n");
	while (1)
	{
		asm volatile("hlt");
	}
}

// #include <stdint.h>
// #include "bootinfo/bootinfo.h"
// #include "pmm.h"
// #include "slab.h"
// #include "vmm.h"

// void test_vmm(void)
// {
// 	printk("\n=== Testing VMM ===\n");

// 	// === 1. Базове мапування сторінки ===
// 	printk("\n--- Test 1: Basic page mapping ---\n");

// 	uint64_t test_virt = 0xFFFF800000001000ULL;
// 	uint64_t test_phys = pmm_alloc_page();

// 	if (test_phys)
// 	{
// 		printk("Allocated physical page: 0x%llx\n", test_phys);

// 		if (vmm_map_page(test_virt, test_phys, PAGE_KERNEL))
// 		{
// 			printk("Mapped 0x%llx -> 0x%llx\n", test_virt, test_phys);

// 			// Перевіряємо мапінг
// 			uint64_t phys_check = vmm_virt_to_phys(test_virt);
// 			printk("Verification: virt_to_phys(0x%llx) = 0x%llx\n",
// 			       test_virt, phys_check);

// 			if (phys_check == test_phys)
// 			{
// 				printk("✓ Mapping verified!\n");
// 			}
// 			else
// 			{
// 				printk("✗ Mapping verification failed!\n");
// 			}

// 			// Тестуємо запис/читання
// 			uint64_t *ptr = (uint64_t *)test_virt;
// 			*ptr = 0xDEADBEEFCAFEBABEULL;

// 			if (*ptr == 0xDEADBEEFCAFEBABEULL)
// 			{
// 				printk("✓ Write/read test passed!\n");
// 			}
// 			else
// 			{
// 				printk("✗ Write/read test failed!\n");
// 			}

// 			// Анмапимо
// 			vmm_unmap_page(test_virt);
// 			printk("Unmapped page\n");
// 		}
// 	}

// 	// === 2. Мапування діапазону ===
// 	printk("\n--- Test 2: Range mapping ---\n");

// 	uint64_t range_virt = 0xFFFF800000010000ULL;
// 	uint64_t range_phys = pmm_alloc_pages(4);

// 	if (range_phys)
// 	{
// 		printk("Allocated 4 physical pages starting at: 0x%llx\n", range_phys);

// 		if (vmm_map_range(range_virt, range_phys, 4 * PAGE_SIZE_4K, PAGE_KERNEL))
// 		{
// 			printk("Mapped range: 0x%llx -> 0x%llx (16KB)\n",
// 			       range_virt, range_phys);

// 			// Перевіряємо кожну сторінку
// 			bool all_mapped = true;
// 			for (int i = 0; i < 4; i++)
// 			{
// 				uint64_t virt = range_virt + i * PAGE_SIZE_4K;
// 				uint64_t phys = vmm_virt_to_phys(virt);
// 				uint64_t expected_phys = range_phys + i * PAGE_SIZE_4K;

// 				if (phys != expected_phys)
// 				{
// 					printk("✗ Page %d: expected 0x%llx, got 0x%llx\n",
// 					       i, expected_phys, phys);
// 					all_mapped = false;
// 				}
// 			}

// 			if (all_mapped)
// 			{
// 				printk("✓ All pages mapped correctly!\n");
// 			}

// 			// Тестуємо запис через весь діапазон
// 			uint64_t *ptr = (uint64_t *)range_virt;
// 			for (int i = 0; i < 512 * 4; i++)
// 			{ // 4 pages * 512 entries
// 				ptr[i] = i;
// 			}

// 			bool write_ok = true;
// 			for (int i = 0; i < 512 * 4; i++)
// 			{
// 				if (ptr[i] != i)
// 				{
// 					write_ok = false;
// 					break;
// 				}
// 			}

// 			if (write_ok)
// 			{
// 				printk("✓ Range write/read test passed!\n");
// 			}
// 			else
// 			{
// 				printk("✗ Range write/read test failed!\n");
// 			}

// 			vmm_unmap_range(range_virt, 4 * PAGE_SIZE_4K);
// 			printk("Unmapped range\n");
// 		}
// 	}

// 	// === 3. vmm_alloc / vmm_free ===
// 	printk("\n--- Test 3: vmm_alloc/vmm_free ---\n");

// 	uint64_t alloc_virt = vmm_alloc(8192, PAGE_KERNEL); // 2 pages
// 	if (alloc_virt)
// 	{
// 		printk("vmm_alloc(8192) = 0x%llx\n", alloc_virt);

// 		// Записуємо щось
// 		uint64_t *ptr = (uint64_t *)alloc_virt;
// 		ptr[0] = 0x1122334455667788ULL;

// 		if (ptr[0] == 0x1122334455667788ULL)
// 		{
// 			printk("✓ vmm_alloc memory is usable!\n");
// 		}

// 		vmm_free(alloc_virt, 8192);
// 		printk("Freed allocated memory\n");
// 	}

// 	// === 4. Зміна флагів ===
// 	printk("\n--- Test 4: Changing page flags ---\n");

// 	uint64_t flags_virt = 0xFFFF800000020000ULL;
// 	uint64_t flags_phys = pmm_alloc_page();

// 	if (flags_phys)
// 	{
// 		// Мапимо як read-only
// 		vmm_map_page(flags_virt, flags_phys, PAGE_READONLY);

// 		uint64_t flags = vmm_get_flags(flags_virt);
// 		printk("Initial flags: 0x%llx (should be read-only)\n", flags);

// 		// Змінюємо на read-write
// 		vmm_set_flags(flags_virt, PAGE_KERNEL);

// 		flags = vmm_get_flags(flags_virt);
// 		printk("Updated flags: 0x%llx (should be read-write)\n", flags);

// 		if (flags & PAGE_WRITE)
// 		{
// 			printk("✓ Flags changed successfully!\n");
// 		}

// 		vmm_unmap_page(flags_virt);
// 	}

// 	// === 5. Створення нового адресного простору ===
// 	printk("\n--- Test 5: Creating new address space ---\n");

// 	address_space_t *new_as = vmm_create_address_space();
// 	if (new_as)
// 	{
// 		printk("Created new address space:\n");
// 		printk("  PML4 physical: 0x%llx\n", new_as->pml4_phys);
// 		printk("  Heap start: 0x%llx\n", new_as->heap_start);
// 		printk("  Stack start: 0x%llx\n", new_as->stack_start);

// 		// Зберігаємо поточний address space
// 		address_space_t *old_as = vmm_get_current_address_space();
// 		printk("Current address space PML4: 0x%llx\n", old_as->pml4_phys);

// 		// Перемикаємось на новий address space
// 		vmm_switch_address_space(new_as);
// 		printk("Switched to new address space\n");

// 		// Перевіряємо що CR3 змінився
// 		uint64_t cr3 = vmm_get_cr3();
// 		if (cr3 == new_as->pml4_phys)
// 		{
// 			printk("✓ Address space switch successful!\n");
// 		}

// 		// Повертаємось назад
// 		vmm_switch_address_space(old_as);
// 		printk("Switched back to original address space\n");

// 		// Видаляємо новий address space
// 		vmm_destroy_address_space(new_as);
// 		printk("Destroyed new address space\n");
// 	}

// 	// === 6. Статистика ===
// 	printk("\n--- Final Statistics ---\n");
// 	vmm_print_stats();
// }

// void kernel_main(BootInfo *boot_info)
// {
// 	ram_info_t *ram_info = boot_info->memory_map;

// 	printk("Kernel started!\n");
// 	printk("Heap: 0x%llx - 0x%llx (%llu MB)\n",
// 	       ram_info->heap_start,
// 	       ram_info->heap_start + ram_info->heap_size,
// 	       ram_info->heap_size / (1024 * 1024));

// 	// === Ініціалізація підсистем ===
// 	printk("\n=== Initializing Memory Management ===\n");

// 	printk("Initializing PMM...\n");
// 	pmm_init(ram_info->heap_start, ram_info->heap_size);
// 	printk("✓ PMM initialized\n");

// 	printk("Initializing Slab Allocator...\n");
// 	slab_init();
// 	printk("✓ Slab Allocator initialized\n");

// 	printk("Initializing VMM...\n");
// 	vmm_init();
// 	printk("✓ VMM initialized\n");

// 	// Статистика після ініціалізації
// 	pmm_info_t *pmm_info = pmm_get_info();
// 	printk("\nMemory after initialization:\n");
// 	printk("  PMM used: %llu MB\n", pmm_info->used_memory / (1024 * 1024));
// 	printk("  PMM free: %llu MB\n",
// 	       (pmm_info->total_memory - pmm_info->used_memory) / (1024 * 1024));

// 	// Тестуємо VMM
// 	test_vmm();

// 	printk("\n=== All tests completed ===\n");

// 	// === Приклад реального використання ===
// 	printk("\n=== Real-world example: Creating kernel heap ===\n");

// 	// Виділяємо 1MB для kernel heap
// 	uint64_t heap_virt = vmm_alloc(1024 * 1024, PAGE_KERNEL);
// 	if (heap_virt)
// 	{
// 		printk("Kernel heap allocated at: 0x%llx (1MB)\n", heap_virt);

// 		// Тепер можна використовувати цю пам'ять для будь-чого
// 		// Наприклад, як backing memory для heap allocator

// 		uint64_t *test_data = (uint64_t *)heap_virt;
// 		test_data[0] = 0xCAFEBABE;
// 		test_data[1] = 0xDEADBEEF;

// 		printk("Written test data to heap\n");
// 		printk("  [0] = 0x%llx\n", test_data[0]);
// 		printk("  [1] = 0x%llx\n", test_data[1]);
// 	}

// 	// Головний цикл ядра
// 	printk("\nEntering main loop...\n");
// 	while (1)
// 	{
// 		asm volatile("hlt");
// 	}
// }

// // Приклад структур для виділення
// typedef struct task
// {
// 	uint64_t id;
// 	uint64_t state;
// 	uint64_t stack_pointer;
// 	uint64_t page_table;
// 	char name[32];
// } task_t;

// typedef struct file_descriptor
// {
// 	uint64_t inode;
// 	uint64_t offset;
// 	uint32_t flags;
// 	uint32_t refcount;
// } fd_t;

// void test_slab_allocator(void)
// {
// 	printk("\n=== Testing Slab Allocator ===\n");

// 	// === 1. Базове використання kmalloc/kfree ===
// 	printk("\n--- Test 1: Basic kmalloc/kfree ---\n");

// 	void *ptr1 = kmalloc(32);
// 	printk("kmalloc(32) = 0x%llx\n", (uint64_t)ptr1);

// 	void *ptr2 = kmalloc(100);
// 	printk("kmalloc(100) = 0x%llx\n", (uint64_t)ptr2);

// 	void *ptr3 = kmalloc(1024);
// 	printk("kmalloc(1024) = 0x%llx\n", (uint64_t)ptr3);

// 	kfree(ptr1);
// 	kfree(ptr2);
// 	kfree(ptr3);
// 	printk("All freed successfully\n");

// 	// === 2. Створення custom cache для структур ===
// 	printk("\n--- Test 2: Custom cache for task_t ---\n");

// 	slab_cache_t *task_cache = slab_cache_create("task_cache",
// 						     sizeof(task_t),
// 						     64);
// 	if (task_cache)
// 	{
// 		printk("Task cache created: object_size=%zu\n", task_cache->object_size);

// 		// Виділяємо кілька task'ів
// 		task_t *task1 = (task_t *)slab_alloc(task_cache);
// 		task_t *task2 = (task_t *)slab_alloc(task_cache);
// 		task_t *task3 = (task_t *)slab_alloc(task_cache);

// 		if (task1 && task2 && task3)
// 		{
// 			printk("Allocated 3 tasks:\n");
// 			printk("  task1 = 0x%llx\n", (uint64_t)task1);
// 			printk("  task2 = 0x%llx\n", (uint64_t)task2);
// 			printk("  task3 = 0x%llx\n", (uint64_t)task3);

// 			// Ініціалізуємо task'и
// 			task1->id = 1;
// 			task1->state = 1;
// 			strcpy(task1->name, "init");

// 			task2->id = 2;
// 			task2->state = 1;
// 			strcpy(task2->name, "kernel_thread");

// 			// Звільняємо один task
// 			slab_free(task_cache, task2);
// 			printk("Freed task2\n");

// 			// Виділяємо знову - повинен повернути той самий адрес
// 			task_t *task4 = (task_t *)slab_alloc(task_cache);
// 			printk("task4 = 0x%llx (should reuse task2's memory)\n",
// 			       (uint64_t)task4);

// 			slab_free(task_cache, task1);
// 			slab_free(task_cache, task3);
// 			slab_free(task_cache, task4);
// 		}
// 	}

// 	// === 3. Тест великих виділень ===
// 	printk("\n--- Test 3: Large allocations ---\n");

// 	void *large1 = kmalloc(8192); // 2 pages
// 	printk("kmalloc(8192) = 0x%llx\n", (uint64_t)large1);

// 	void *large2 = kmalloc(1024 * 1024); // 256 pages (1MB)
// 	printk("kmalloc(1MB) = 0x%llx\n", (uint64_t)large2);

// 	kfree(large1);
// 	kfree(large2);
// 	printk("Large allocations freed\n");

// 	// === 4. Стрес-тест ===
// 	printk("\n--- Test 4: Stress test ---\n");

// #define NUM_ALLOCS 100
// 	void *ptrs[NUM_ALLOCS];

// 	printk("Allocating %d objects...\n", NUM_ALLOCS);
// 	for (int i = 0; i < NUM_ALLOCS; i++)
// 	{
// 		ptrs[i] = kmalloc(64);
// 		if (!ptrs[i])
// 		{
// 			printk("Allocation %d failed!\n", i);
// 			break;
// 		}
// 	}

// 	printk("Freeing all objects...\n");
// 	for (int i = 0; i < NUM_ALLOCS; i++)
// 	{
// 		if (ptrs[i])
// 		{
// 			kfree(ptrs[i]);
// 		}
// 	}
// 	printk("Stress test completed\n");

// 	// === 5. Вирівняне виділення ===
// 	printk("\n--- Test 5: Aligned allocation ---\n");

// 	void *aligned = kmalloc_aligned(256, 256);
// 	printk("kmalloc_aligned(256, 256) = 0x%llx\n", (uint64_t)aligned);

// 	if ((uint64_t)aligned % 256 == 0)
// 	{
// 		printk("Alignment verified!\n");
// 	}
// 	else
// 	{
// 		printk("ERROR: Alignment failed!\n");
// 	}

// 	kfree(aligned);

// 	// === 6. Статистика ===
// 	printk("\n--- Final Statistics ---\n");
// 	slab_print_stats();

// 	pmm_info_t *pmm_info = pmm_get_info();
// 	printk("\nPMM Statistics:\n");
// 	printk("  Used memory: %llu MB\n", pmm_info->used_memory / (1024 * 1024));
// 	printk("  Free memory: %llu MB\n",
// 	       (pmm_info->total_memory - pmm_info->used_memory) / (1024 * 1024));
// }

// void kernel_main(BootInfo *boot_info)
// {
// 	ram_info_t *ram_info = boot_info->memory_map;

// 	printk("Kernel started!\n");

// 	// Ініціалізуємо PMM
// 	printk("\n=== Initializing PMM ===\n");
// 	pmm_init(ram_info->heap_start, ram_info->heap_size);
// 	printk("PMM initialized\n");

// 	// Ініціалізуємо Slab Allocator
// 	printk("\n=== Initializing Slab Allocator ===\n");
// 	slab_init();
// 	printk("Slab allocator initialized\n");

// 	// Запускаємо тести
// 	test_slab_allocator();

// 	printk("\n=== All tests completed ===\n");

// 	// Приклад реального використання
// 	printk("\n=== Real-world example ===\n");

// 	// Створюємо cache для файлових дескрипторів
// 	slab_cache_t *fd_cache = slab_cache_create("fd_cache", sizeof(fd_t), 8);

// 	// Відкриваємо кілька файлів
// 	fd_t *fd1 = (fd_t *)slab_alloc(fd_cache);
// 	fd_t *fd2 = (fd_t *)slab_alloc(fd_cache);

// 	if (fd1 && fd2)
// 	{
// 		fd1->inode = 100;
// 		fd1->offset = 0;
// 		fd1->flags = 0x01; // READ

// 		fd2->inode = 200;
// 		fd2->offset = 0;
// 		fd2->flags = 0x02; // WRITE

// 		printk("Created file descriptors:\n");
// 		printk("  fd1: inode=%llu, offset=%llu\n", fd1->inode, fd1->offset);
// 		printk("  fd2: inode=%llu, offset=%llu\n", fd2->inode, fd2->offset);

// 		// Закриваємо файли
// 		slab_free(fd_cache, fd1);
// 		slab_free(fd_cache, fd2);
// 	}

// 	// Головний цикл ядра
// 	while (1)
// 	{
// 		asm volatile("hlt");
// 	}
// }

// void kernel_main(BootInfo *bi)
// {
// 	// Инициализация раннего printk
// 	early_printk_init(bi->framebuffer);

// 	printk(KERN_INFO "Kernel starting...\n");

// 	// ========================================================================
// 	// Инициализация GDT, TSS, IDT
// 	// ========================================================================

// 	printk(KERN_INFO "GDT init\n");
// 	gdt_init();

// 	printk(KERN_INFO "TSS init\n");
// 	tss_init();

// 	printk(KERN_INFO "IDT init\n");
// 	idt_init();

// 	printk(KERN_INFO "Interrupts init\n");
// 	interrupts_init(); // Перепрограммирует PIC и включает прерывания

// 	printk(KERN_INFO "Syscalls init\n");
// 	syscall_init();

// 	// ========================================================================
// 	// Установка обработчиков
// 	// ========================================================================

// 	// IRQ 0 - Timer
// 	// irq_install_handler(0, timer_handler);
// 	// irq_clear_mask(0); // Разрешаем прерывание таймера

// 	// IRQ 1 - Keyboard
// 	// irq_install_handler(1, keyboard_handler);
// 	// irq_clear_mask(1); // Разрешаем прерывание клавиатуры

// 	printk(KERN_INFO "Interrupt handlers installed\n");

// 	// ========================================================================
// 	// Остальная инициализация
// 	// ========================================================================

// 	printk(KERN_INFO "VMM init\n");
// 	uint64_t cr3;
// 	asm volatile("mov %%cr3, %0" : "=r"(cr3));
// 	vmm_init(cr3, bi->memory_map->heap_start, bi->memory_map->heap_size);

// 	printk(KERN_INFO "PMM init\n");
// 	pmm_init(bi->memory_map->heap_start, bi->memory_map->heap_size);

// 	// DISABLE BOOTSTRAP
// 	vmm_disable_bootstrap_allocator();

// 	printk(KERN_INFO "kmalloc init\n");
// 	kmalloc_init();

// 	printk("Testing heap write access...\n");
// 	volatile uint64_t *test_ptr = (uint64_t *)(bi->memory_map->heap_start + 0x1000);
// 	*test_ptr = 0xDEADBEEF;
// 	if (*test_ptr == 0xDEADBEEF)
// 	{
// 		printk("  Heap write test: OK\n");
// 	}
// 	else
// 	{
// 		printk("  Heap write test: FAILED!\n");
// 	}

// 	printk(KERN_INFO "GPT init\n");
// 	gpt_init(partitions);

// 	if (!partitions[1].device->read)
// 	{
// 		partitions[1].device->read = &ata_read_sector;
// 		partitions[1].device->write = &ata_write_sector;
// 		partitions[1].device->device = &ata_devices[0];
// 	}
// 	printk("FAT32 init at LBA %d\n", partitions[0].first_lba);
// 	vfs_mount("/", &partitions[0], FS_FAT32);

// 	// printk("FAT32 mounted\n");
// 	// printk("root cluster: %d\n", ((FAT32_FS *)(root_fs->fs))->root_cluster);
// 	// vfs_create_file("/tesit.txt");
// 	// Directory dir = vfs_readdir("/");

// 	// for (int i = 0; i < dir.count; i++)
// 	// {
// 	// 	printk("%s %d\n", dir.entries[i].name, dir.entries[i].is_dir);
// 	// }
// 	// dir.free_entries(&dir);
// 	// printk("EXT2 init\n");

// 	// vfs_mount("/mnt/ext2", &partitions[1], FS_EXT2);
// 	// Directory dir = vfs_readdir("/mnt/ext2");
// 	// printk("trying %d count", dir.count);
// 	// for (int i = 0; i < dir.count; i++)
// 	// {
// 	// 	printk("%s %d\n", dir.entries[i].name, dir.entries[i].is_dir);
// 	// }
// 	// VFS_File *f = vfs_open("/tesiit.txt", VFS_O_RDONLY);
// 	// vfs_write(f, "Hello wo123", 11);
// 	// vfs_lseek(f, 0, SEEK_SET);
// 	// printk("Reading file: ");
// 	// vfs_read(f, buffer, 1024);
// 	// printk("File content: ");
// 	// printk("%s\n", buffer);
// 	// vfs_lseek(f, 0, SEEK_SET);
// 	// vfs_close(&f);
// 	// if (f == NULL)
// 	// {
// 	// 	printk("VFS: file was not closed kmain%s\n", f->node->name);
// 	// }
// 	// vfs_read(f, buffer, 1024);
// 	// printk("FAT32 init done\n");

// 	// ext2_init(partitions[1]);

// 	// printk(KERN_INFO "FAT32 init at LBA %d\n", partitions[0].first_lba);
// 	// vfs_mount(&partitions[0], FS_FAT32);

// 	printk(KERN_INFO "FAT32 mounted\n");
// 	// printk(KERN_INFO "root cluster: %d\n", ((FAT32_FS *)(root_fs->fs))->root_cluster);

// 	// Directory dir = vfs_readdir("/");
// 	// 	// for (int i = 0; i < dir.count; i++)
// 	// 	// {
// 	// 	// 	printk("%s %d\n", dir.entries[i].name, dir.entries[i].is_dir);
// 	// 	// }
// 	// 	// dir.free_entries(&dir);
// 	// 	// VFS_File *f = vfs_open("/tesit.txt", VFS_O_CREAT | VFS_O_RDWR);
// 	// 	// vfs_write(f, "Hello wo123", 11);
// 	// 	// vfs_lseek(f, 0, SEEK_SET);
// 	// 	// printk("Reading file: ");
// 	// 	// vfs_read(f, buffer, 1024);
// 	// 	// printk("File content: ");
// 	// 	// for (int i = 0; i < 1024; i++)
// 	// 	// {
// 	// 	// 	if (buffer[i] == '\0')
// 	// 	// 		break;
// 	// 	// 	printk("%c", buffer[i]);
// 	// 	// }

// 	printk(KERN_INFO "Kernel initialization complete\n");

// 	// ========================================================================
// 	// Тест прерываний
// 	// ========================================================================

// 	printk(KERN_INFO "Testing interrupts...\n");

// 	uint16_t cs, ds;
// 	asm volatile("mov %%cs, %0" : "=r"(cs));
// 	asm volatile("mov %%ds, %0" : "=r"(ds));
// 	printk("CS: 0x%x, DS: 0x%x\n", cs, ds);

// 	// Тест breakpoint (INT 3)
// 	// asm volatile("int3");

// 	// Тест системного вызова

// 	printk(KERN_INFO "Test 1: syscall(0) - expecting 666\n");
// 	uint64_t result;

// 	asm volatile(
// 	    "movq $0, %%rax\n"
// 	    "int $0x80\n"
// 	    // "syscall\n"
// 	    "movq %%rax, %0"
// 	    : "=r"(result)
// 	    :
// 	    : "rax");

// 	printk(KERN_INFO " Result: %lu (0x%lx)\n", result, result);

// 	// printk(KERN_INFO "Test 1: syscall(0) - expecting 666\n");

// 	// uint64_t result;
// 	// asm volatile(
// 	//     "movq $0, %%rax\n"
// 	//     "syscall\n"
// 	//     "movq %%rax, %0\n"
// 	//     : "=r"(result)
// 	//     :
// 	//     : "rax", "rcx", "r11", "memory");

// 	// printk(KERN_INFO "  Result: %lu (0x%lx)\n", result, result);

// 	// printk(KERN_INFO "Test 2: syscall(5, 0, 0, 0, 0, 0, 0) - expecting 666\n");
// 	// long ret;
// 	// long num = 5;
// 	// long arg1 = 0;
// 	// long arg2 = 0;
// 	// long arg3 = 0;
// 	// long arg4 = 0;
// 	// long arg5 = 0;
// 	// long arg6 = 0;

// 	// // for x86_64 с syscall instruction
// 	// __asm__ volatile(
// 	//     "movq %1, %%rax\n" // num syscall
// 	//     "movq %2, %%rdi\n" // arg1
// 	//     "movq %3, %%rsi\n" // arg2
// 	//     "movq %4, %%rdx\n" // arg3
// 	//     "movq %5, %%r10\n" // arg4
// 	//     "movq %6, %%r8\n"  // arg5
// 	//     "movq %7, %%r9\n"  // arg6
// 	//     "syscall\n"	       // or int $0x80
// 	// 		       //     "int $0x80\n"
// 	//     "movq %%rax, %0\n" // result
// 	//     : "=r"(ret)
// 	//     : "r"(num), "r"(arg1), "r"(arg2),
// 	//       "r"(arg3), "r"(arg4), "r"(arg5), "r"(arg6)
// 	//     : "rax", "rdi", "rsi", "rdx", "r10", "r8", "r9", "memory");

// 	// printk(KERN_INFO "  Result: %ld (0x%lx)\n", ret, ret);

// 	// ========================================================================
// 	// Основной цикл
// 	// ========================================================================

// 	os_main(bi);

// 	printk(KERN_INFO "Entering main loop\n");

// 	while (1)
// 	{
// 		// Ждем прерываний
// 		asm volatile("hlt");
// 	}
// }
