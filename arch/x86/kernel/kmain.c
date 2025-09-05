#include "utils/framebuffer.h"
#include "utils/font.h"
#include "heap.h"
#include "utils/fat32/fat.h"
#include "utils/fat32/fat_structs.h"
#include "utils/ata/ata.h"
#include "utils/gpt/gpt.h"
#include "utils/gpt/gpt_struct.h"
#include "utils/vfs/vfs_standart_struct.h"
#include "utils/vfs/vfs.h"

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
static gpt_partition_t partitions[20] = {
    {.device = &ata_devices[0],
     .type = 0}};
extern void os_main(framebuffer_info_t *fb);
extern void libc_init(void);
extern VFS_FS *root_fs;

void kernel_main(BootInfo *bi)
{
	heap_init(bi->memory_map->heap_start, bi->memory_map->heap_size);
	framebuffer_info_t *fb = bi->framebuffer;

	init_font(fb);
	libc_init();
	printf("GPT init\n");

	gpt_init(partitions);

	printf("FAT32 init at LBA %d\n", partitions[0].first_lba);
	VFS_Device *device = malloc(sizeof(VFS_Device));
	device->type = DEV_ATA;
	device->device = &ata_devices[0];
	vfs_mount(device, partitions[0].first_lba, FS_FAT32);
	printf("FAT32 mounted\n");
	printf("root cluster: %d\n", ((FAT32_FS *)(root_fs->fs))->root_cluster);
	VFS_File *f = vfs_open("/tesiit.txt", VFS_O_CREAT | VFS_O_RDWR);
	// vfs_write(f, "Hello, world!", 13);
	vfs_lseek(f, 0, SEEK_SET);
	Directory dir = vfs_readdir("/");
	for (int i = 0; i < dir.count; i++)
	{
		printf("%s %d\n", dir.entries[i].name, dir.entries[i].is_dir);
	}

	char buffer[1024];
	vfs_read(f, buffer, 1024);
	printf("Read: \n");
	for (int i = 0; i < 1024; i++)
	{
		printf("%c", buffer[i]);
	}

	printf("FAT32 init done\n");

	while (1)
	{
		;
	}
}
