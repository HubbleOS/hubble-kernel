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
#include "utils/bwfvideo.h"
#include "utils/nvme/nvme.h"
#include "utils/pci/pci.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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
extern void os_main(framebuffer_info_t *fb);
extern void libc_init(void);
extern VFS_FS *root_fs;
BootInfo boot_info;

void kernel_main(BootInfo *bi)
{
	boot_info = *bi;
	heap_init(bi->memory_map->heap_start, bi->memory_map->heap_size);
	framebuffer_info_t *fb = bi->framebuffer;

	init_font(fb);
	libc_init();

	char buffer[1024];

	printf("GPT init\n");

	// struct pci_device *nvme = find_nvme_qemu();
	// printf("NVMe bus: %d", nvme->bus);

	printf("NVMe init\n");
	gpt_init(partitions);

	printf("FAT32 init at LBA %d\n", partitions[0].first_lba);
	vfs_mount(&partitions[0], FS_FAT32);
	printf("FAT32 mounted\n");
	printf("root cluster: %d\n", ((FAT32_FS *)(root_fs->fs))->root_cluster);

	VFS_File *f = vfs_open("/tesiit.txt", VFS_O_CREAT | VFS_O_RDWR);
	vfs_lseek(f, 0, SEEK_SET);
	Directory dir = vfs_readdir("/");

	for (int i = 0; i < dir.count; i++)
	{
		printf("%s %d\n", dir.entries[i].name, dir.entries[i].is_dir);
	}
	dir.free_entries(&dir);

	vfs_read(f, buffer, 1024);

	printf("FAT32 init done\n");

	while (1)
	{
		;
	}
}
