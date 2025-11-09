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

#include <mm/kmalloc.h>
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

	// Получаем текущий CR3 от bootloader
	uint64_t cr3;
	asm volatile("mov %%cr3, %0" : "=r"(cr3));
	printk("Current CR3: 0x%lx\n", cr3);

	// Выводим информацию о heap
	printk("Heap region: 0x%lx - 0x%lx (%lu MB)\n",
	       bi->memory_map->heap_start,
	       bi->memory_map->heap_start + bi->memory_map->heap_size,
	       bi->memory_map->heap_size / (1024 * 1024));

	// ШАГ 1: PMM init
	// PMM работает через identity mapping от UEFI
	// Bitmap будет в начале heap региона
	printk(KERN_INFO "PMM init (using UEFI identity mapping)...\n");
	pmm_init(bi->memory_map->heap_start, bi->memory_map->heap_size);

	// ШАГ 2: VMM init
	// VMM создает physmap, используя PMM для аллокации page tables
	printk(KERN_INFO "VMM init (creating physmap)...\n");
	vmm_init(cr3, bi->memory_map->heap_start, bi->memory_map->heap_size);

	// ШАГ 2.5: Включаем physmap в PMM
	// После создания physmap, PMM может использовать его вместо identity mapping
	printk(KERN_INFO "Enabling physmap in PMM...\n");
	// pmm_enable_physmap();

	// ШАГ 3: kmalloc init
	// kmalloc использует PMM/VMM для выделения памяти
	printk(KERN_INFO "kmalloc init...\n");
	kmalloc_init();

	while (1)
	{
		asm volatile("hlt");
	}

	// ========================================================================
	// Тесты памяти
	// ========================================================================
	printk(KERN_INFO "=== Memory Tests ===\n");

	// Тест 1: Identity mapping (от UEFI)
	printk("Test 1: Identity mapping write...\n");
	volatile uint64_t *test_id = (uint64_t *)(bi->memory_map->heap_start + 0x1000);
	*test_id = 0xDEADBEEF;
	if (*test_id == 0xDEADBEEF)
	{
		printk("  Identity mapping: OK\n");
	}
	else
	{
		printk("  Identity mapping: FAILED!\n");
	}

	// Тест 2: PMM аллокация
	printk("Test 2: PMM allocation...\n");
	uint64_t phys = pmm_alloc_phys(1);
	if (phys)
	{
		printk("  Allocated physical page: 0x%lx\n", phys);

		// Проверяем запись через physmap
		uint64_t virt = phys + 0xFFFF800000000000ULL;
		volatile uint64_t *ptr = (uint64_t *)virt;
		*ptr = 0xCAFEBABE;

		if (*ptr == 0xCAFEBABE)
		{
			printk("  Physmap write: OK\n");
		}
		else
		{
			printk("  Physmap write: FAILED!\n");
		}

		pmm_free_phys(phys, 1);
		printk("  Page freed\n");
	}
	else
	{
		printk("  PMM allocation: FAILED!\n");
	}

	// Тест 3: kmalloc
	printk("Test 3: kmalloc...\n");
	void *ptr = kmalloc(1024);
	if (ptr)
	{
		printk("  kmalloc(1024): 0x%lx\n", (uint64_t)ptr);
		memset(ptr, 0xAB, 1024);
		printk("  memset: OK\n");
		kfree(ptr);
		printk("  kfree: OK\n");
	}
	else
	{
		printk("  kmalloc: FAILED!\n");
	}

	// Тест 4: Большая аллокация
	printk("Test 4: Large allocation (1 MB)...\n");
	void *large = kmalloc(1024 * 1024);
	if (large)
	{
		printk("  kmalloc(1 MB): 0x%lx\n", (uint64_t)large);

		// Проверяем что можем писать
		volatile uint32_t *test_array = (uint32_t *)large;
		for (int i = 0; i < 256; i++)
		{
			test_array[i] = 0x12345678 + i;
		}

		// Проверяем чтение
		int ok = 1;
		for (int i = 0; i < 256; i++)
		{
			if (test_array[i] != 0x12345678 + i)
			{
				ok = 0;
				break;
			}
		}

		if (ok)
		{
			printk("  Read/write test: OK\n");
		}
		else
		{
			printk("  Read/write test: FAILED!\n");
		}

		kfree(large);
		printk("  Large block freed\n");
	}
	else
	{
		printk("  Large allocation: FAILED!\n");
	}

	// Статистика PMM
	size_t free_pages = pmm_get_free_pages();
	size_t total_pages = pmm_get_total_pages();
	printk("PMM Statistics:\n");
	printk("  Free:  %lu pages (%lu MB)\n", free_pages, free_pages * 4 / 1024);
	printk("  Total: %lu pages (%lu MB)\n", total_pages, total_pages * 4 / 1024);
	printk("  Used:  %lu pages (%lu MB)\n",
	       total_pages - free_pages, (total_pages - free_pages) * 4 / 1024);

	// ========================================================================
	// Остальная инициализация
	// ========================================================================
	printk(KERN_INFO "GPT init\n");
	gpt_init(partitions);

	printk(KERN_INFO "FAT32 init\n");
	vfs_mount("/", &partitions[0], FS_FAT32);

	printk(KERN_INFO "Kernel initialization complete\n");

	// ========================================================================
	// Тесты прерываний и системных вызовов
	// ========================================================================
	printk(KERN_INFO "=== Interrupt Tests ===\n");

	uint16_t cs, ds;
	asm volatile("mov %%cs, %0" : "=r"(cs));
	asm volatile("mov %%ds, %0" : "=r"(ds));
	printk("CS: 0x%x, DS: 0x%x\n", cs, ds);

	printk(KERN_INFO "Test syscall(0) - expecting 666\n");
	uint64_t result;

	asm volatile(
	    "movq $0, %%rax\n"
	    "int $0x80\n"
	    "movq %%rax, %0"
	    : "=r"(result)
	    :
	    : "rax");

	printk(KERN_INFO "Result: %lu (0x%lx)\n", result, result);

	// ========================================================================
	// Переход в основной код ядра
	// ========================================================================
	os_main(bi);

	printk(KERN_INFO "Entering main loop\n");
	while (1)
	{
		asm volatile("hlt");
	}
}

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
