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
	framebuffer_info_t *fb = bi->framebuffer;

	for (int i = 0; i < fb->width * fb->height; i++)
	{
		((uint32_t *)fb->base)[i] = rgb(0, 0, 0);
	}

	init_font(fb);
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
	vmm_test();

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
	while (1)
		;
}
