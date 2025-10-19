#include "utils/framebuffer.h"
#include "utils/font.h"
#include "utils/fat32/fat.h"
#include "utils/fat32/fat_structs.h"
#include "utils/ata/ata.h"
#include "utils/gpt/gpt.h"
#include "utils/gpt/gpt_struct.h"
#include "utils/vfs/vfs_standart_struct.h"
#include "utils/vfs/vfs.h"

#include <mm/pmm.h>
#include <mm/heap.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static ATA_Device ata_devices[2] = {
    {
	.bus = 0,
	.device = 0,
	.io_base = 0x1F0,
	.ctrl_base = 0x3F6,
	.read = ata_read_sector,
	.write = ata_write_sector,
    },
    {
	.bus = 1,
	.device = 0,
	.io_base = 0x170,
	.ctrl_base = 0x376,
	.read = ata_read_sector,
	.write = ata_write_sector,
    },
};

static gpt_partition_t partitions[20] =
    {
	{
	    .device = &ata_devices[0],
	    .type = 0,
	},
};

extern void os_main(framebuffer_info_t *fb);
extern void libc_init(void);
extern VFS_FS *root_fs;

#include <mm/kmalloc.h>

// ========== TEST ==========
// void test_kmalloc(void)
// {
// 	printf("\n=== Testing kmalloc ===\n");

// 	// Test 1: Simple allocation
// 	printf("\nTest 1: Simple allocation\n");
// 	void *p1 = kmalloc(100);
// 	void *p2 = kmalloc(200);
// 	void *p3 = kmalloc(500);
// 	printf("p1=%p, p2=%p, p3=%p\n", p1, p2, p3);

// 	// Test 2: Write and read
// 	printf("\nTest 2: Write and read\n");
// 	char *str = (char *)kmalloc(50);
// 	strcpy(str, "Hello, kernel!");
// 	printf("String: %s\n", str);

// 	// Test 3: Free and realloc
// 	printf("\nTest 3: Free and realloc\n");
// 	kfree(p2);
// 	void *p4 = kmalloc(200);
// 	printf("p4=%p (should reuse p2's space)\n", p4);

// 	// Test 4: Zero allocation
// 	printf("\nTest 4: Zero allocation\n");
// 	int *arr = (int *)kzalloc(10 * sizeof(int));
// 	printf("Array: ");
// 	for (int i = 0; i < 10; i++)
// 		printf("%d ", arr[i]);
// 	printf("\n");

// 	// Test 5: Realloc
// 	printf("\nTest 5: Realloc\n");
// 	char *small = (char *)kmalloc(10);
// 	strcpy(small, "Small");
// 	char *large = (char *)krealloc(small, 100);
// 	printf("After realloc: %s\n", large);

// 	// Test 6: Large allocation
// 	printf("\nTest 6: Large allocation\n");
// 	void *big = kmalloc(1024 * 1024); // 1MB
// 	printf("Big allocation: %p\n", big);

// 	// Cleanup
// 	kfree(p1);
// 	kfree(p3);
// 	kfree(p4);
// 	kfree(str);
// 	kfree(arr);
// 	kfree(large);
// 	kfree(big);

// 	// Stats
// 	kmalloc_stats();

// 	printf("=== kmalloc tests complete ===\n");
// }

void kernel_main(BootInfo *bi)
{
	// heap_init(bi->memory_map->heap_start, bi->memory_map->heap_size);

	init_font(bi->framebuffer);
	libc_init();
	putchar('\n');

	// 1. Получаем текущий CR3 от UEFI
	uint64_t cr3;
	asm volatile("mov %%cr3, %0" : "=r"(cr3));

	// 2. VMM init БЕЗ использования PMM
	//    Он маппит heap используя UEFI page tables
	vmm_init(cr3, bi->memory_map->heap_start, bi->memory_map->heap_size);

	// 3. ТОЛЬКО после mapping heap можно инициализировать PMM
	pmm_init(bi->memory_map->heap_start, bi->memory_map->heap_size);

	// 4. Теперь можно тестировать
	// vmm_test();

	printf("kmalloc init\n");
	kmalloc_init();

	// test_kmalloc();

	// void *p1 = kmalloc(100);
	// void *p2 = kmalloc(200);
	// void *p3 = kmalloc(500);
	// printf("p1=%p, p2=%p, p3=%p\n", p1, p2, p3);

	// kfree(p2);
	// void *p4 = kmalloc(200);
	// printf("p4=%p (should reuse p2's space)\n", p4);

	// kmalloc_stats();

	// printf("GPT init\n");
	// gpt_init(partitions);

	// printf("FAT32 init at LBA %d\n", partitions[0].first_lba);
	// VFS_Device *device = malloc(sizeof(VFS_Device));
	// device->type = DEV_ATA;
	// device->device = &ata_devices[0];
	// vfs_mount(device, partitions[0].first_lba, FS_FAT32);
	// printf("FAT32 mounted\n");
	// printf("root cluster: %d\n", ((FAT32_FS *)(root_fs->fs))->root_cluster);
	// VFS_File *f = vfs_open("/test.txt", VFS_O_CREAT | VFS_O_RDWR);
	// // vfs_write(f, "Hello, world!", 13);
	// // vfs_lseek(f, 0, SEEK_SET);
	// Directory dir = vfs_readdir("/");
	// for (int i = 0; i < dir.count; i++)
	// {
	// 	printf("%s %d\n", dir.entries[i].name, dir.entries[i].is_dir);
	// }

	// char buffer[1024];
	// vfs_read(f, buffer, 1024);
	// printf("Read: \n");
	// for (int i = 0; i < 1024; i++)
	// {
	// 	printf("%c", buffer[i]);
	// }

	// printf("\nFAT32 init done\n");

	while (1)
		;
}
