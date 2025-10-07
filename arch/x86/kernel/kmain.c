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

void kernel_main(BootInfo *bi)
{
	// heap_init(bi->memory_map->heap_start, bi->memory_map->heap_size);
	pmm_init(bi->memory_map->heap_start, bi->memory_map->heap_size);
	vmm_init();
	framebuffer_info_t *fb = bi->framebuffer;

	init_font(fb);
	libc_init();
	putchar('\n');

	// alloc page:
	void *p1 = pmm_alloc(1);
	printf("p1: %p\n", p1);

	void *p2 = pmm_alloc(1);
	printf("p2: %p\n", p2);

	pmm_free(p1, 32);
	pmm_free(p2, 32);

	void *p3 = pmm_alloc(1);
	printf("p3: %p\n", p3);

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
	// vfs_write(f, "Hello, world!", 13);
	// vfs_lseek(f, 0, SEEK_SET);
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

	// printf("FAT32 init done\n");

	// void *p1 = malloc(1);
	// printf("p1 = %p\n", p1);

	// void *p2 = malloc(1);
	// printf("p2 = %p\n", p2);

	// free(p1);
	// printf("p1 freed\n");

	// void *p3 = malloc(1);
	// printf("p3 = %p\n", p3);

	// void *p1 = pmm_alloc_pages(10);
	// printf("p1 = %p\n", p1);

	// void *p2 = pmm_alloc_pages(10);
	// printf("p2 = %p\n", p2);

	// pmm_free_pages(p1, 10);
	// printf("p1 freed\n");

	// void *p3 = pmm_alloc_pages(10);
	// printf("p3 = %p\n", p3);

	// print_memory_status();

	// void *p1 = malloc(1000); // 0x1813cf4
	// printf("p1 = %p\n", p1);

	// // // print_memory_status();

	// void *p2 = malloc(9000); // 0x1819cf4
	// printf("p2 = %p\n", p2);

	// // // print_memory_status();

	// free(p2);
	// printf("p2 freed\n");

	// // // print_memory_status();

	// void *p3 = malloc(4096);
	// printf("p3 = %p\n", p3);

	// free(p3);
	// printf("p3 freed\n");

	// // print_memory_status();

	// void *p4 = malloc(1);
	// printf("p4 = %p\n", p4);

	// void *p5 = malloc(1);
	// printf("p5 = %p\n", p5);

	// // print_memory_status();

	// void *arr[10];

	// for (int i = 0; i < 10; i++)
	// {
	// 	arr[i] = malloc(4098);
	// 	printf("arr[%d] = %p\n", i, arr[i]);
	// }

	// // print_memory_status();

	// for (int i = 0; i < 10; i++)
	// {
	// 	free(arr[i]);
	// 	printf("arr[%d] freed\n", i);
	// }

	// print_memory_status();

	while (1)
	{
		;
	}
}
