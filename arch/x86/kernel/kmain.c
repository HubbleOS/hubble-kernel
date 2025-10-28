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
extern void libc_init(void);
extern VFS_FS *root_fs;
BootInfo boot_info;

extern void syscall_init(void);

#include "gdt/gdt.h"
#include "gdt/interrupt.h"

extern void keyboard_handler(registers_t *regs);

void kernel_main(BootInfo *bi)
{
	// Инициализация раннего printk
	early_printk_init(bi->framebuffer);

	printk(KERN_INFO "Kernel starting...\n");

	// ========================================================================
	// Инициализация GDT, TSS, IDT
	// ========================================================================

	printk(KERN_INFO "GDT init\n");
	gdt_init();

	printk(KERN_INFO "TSS init\n");
	tss_init();

	printk(KERN_INFO "IDT init\n");
	idt_init();

	printk(KERN_INFO "Interrupts init\n");
	interrupts_init(); // Перепрограммирует PIC и включает прерывания

	syscall_init();

	// ========================================================================
	// Установка обработчиков
	// ========================================================================

	// IRQ 0 - Timer
	// irq_install_handler(0, timer_handler);
	// irq_clear_mask(0); // Разрешаем прерывание таймера

	// IRQ 1 - Keyboard
	irq_install_handler(1, keyboard_handler);
	irq_clear_mask(1); // Разрешаем прерывание клавиатуры

	printk(KERN_INFO "Interrupt handlers installed\n");

	// ========================================================================
	// Остальная инициализация
	// ========================================================================

	printk(KERN_INFO "VMM init\n");
	uint64_t cr3;
	asm volatile("mov %%cr3, %0" : "=r"(cr3));
	vmm_init(cr3, bi->memory_map->heap_start, bi->memory_map->heap_size);

	printk(KERN_INFO "PMM init\n");
	pmm_init(bi->memory_map->heap_start, bi->memory_map->heap_size);

	printk(KERN_INFO "kmalloc init\n");
	kmalloc_init();

	printk(KERN_INFO "GPT init\n");
	gpt_init(partitions);

	printk(KERN_INFO "FAT32 init at LBA %d\n", partitions[0].first_lba);
	vfs_mount(&partitions[0], FS_FAT32);

	printk(KERN_INFO "FAT32 mounted\n");
	printk(KERN_INFO "root cluster: %d\n", ((FAT32_FS *)(root_fs->fs))->root_cluster);

	// Directory dir = vfs_readdir("/");
	// 	// for (int i = 0; i < dir.count; i++)
	// 	// {
	// 	// 	printk("%s %d\n", dir.entries[i].name, dir.entries[i].is_dir);
	// 	// }
	// 	// dir.free_entries(&dir);
	// 	// VFS_File *f = vfs_open("/tesit.txt", VFS_O_CREAT | VFS_O_RDWR);
	// 	// vfs_write(f, "Hello wo123", 11);
	// 	// vfs_lseek(f, 0, SEEK_SET);
	// 	// printk("Reading file: ");
	// 	// vfs_read(f, buffer, 1024);
	// 	// printk("File content: ");
	// 	// for (int i = 0; i < 1024; i++)
	// 	// {
	// 	// 	if (buffer[i] == '\0')
	// 	// 		break;
	// 	// 	printk("%c", buffer[i]);
	// 	// }

	printk(KERN_INFO "Kernel initialization complete\n");

	// ========================================================================
	// Тест прерываний
	// ========================================================================

	printk(KERN_INFO "Testing interrupts...\n");

	uint16_t cs, ds;
	asm volatile("mov %%cs, %0" : "=r"(cs));
	asm volatile("mov %%ds, %0" : "=r"(ds));
	printk("CS: 0x%x, DS: 0x%x\n", cs, ds);

	// Тест breakpoint (INT 3)
	// asm volatile("int3");

	// Тест системного вызова

	printk(KERN_INFO "Test 1: syscall(0) - expecting 666\n");
	uint64_t result;

	asm volatile(
	    "movq $0, %%rax\n"
	    "int $0x80\n"
	    // "syscall\n"
	    "movq %%rax, %0"
	    : "=r"(result)
	    :
	    : "rax");

	printk(KERN_INFO " Result: %lu (0x%lx)\n", result, result);

	// printk(KERN_INFO "Test 1: syscall(0) - expecting 666\n");

	// uint64_t result;
	// asm volatile(
	//     "movq $0, %%rax\n"
	//     "syscall\n"
	//     "movq %%rax, %0\n"
	//     : "=r"(result)
	//     :
	//     : "rax", "rcx", "r11", "memory");

	// printk(KERN_INFO "  Result: %lu (0x%lx)\n", result, result);

	// printk(KERN_INFO "Test 2: syscall(5, 0, 0, 0, 0, 0, 0) - expecting 666\n");
	// long ret;
	// long num = 5;
	// long arg1 = 0;
	// long arg2 = 0;
	// long arg3 = 0;
	// long arg4 = 0;
	// long arg5 = 0;
	// long arg6 = 0;

	// // for x86_64 с syscall instruction
	// __asm__ volatile(
	//     "movq %1, %%rax\n" // num syscall
	//     "movq %2, %%rdi\n" // arg1
	//     "movq %3, %%rsi\n" // arg2
	//     "movq %4, %%rdx\n" // arg3
	//     "movq %5, %%r10\n" // arg4
	//     "movq %6, %%r8\n"  // arg5
	//     "movq %7, %%r9\n"  // arg6
	//     "syscall\n"	       // or int $0x80
	// 		       //     "int $0x80\n"
	//     "movq %%rax, %0\n" // result
	//     : "=r"(ret)
	//     : "r"(num), "r"(arg1), "r"(arg2),
	//       "r"(arg3), "r"(arg4), "r"(arg5), "r"(arg6)
	//     : "rax", "rdi", "rsi", "rdx", "r10", "r8", "r9", "memory");

	// printk(KERN_INFO "  Result: %ld (0x%lx)\n", ret, ret);

	// ========================================================================
	// Основной цикл
	// ========================================================================

	os_main(bi);

	printk(KERN_INFO "Entering main loop\n");

	while (1)
	{
		// Ждем прерываний
		asm volatile("hlt");
	}
}
