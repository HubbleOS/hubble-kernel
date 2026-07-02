/*
 * Filesystem initialisation — GPT probing, VFS mount setup.
 */

#include <hubble/init.h>
#include <hubble/printk.h>

#include <drivers/storage/ata/ata.h>
#include <fs/gpt/gpt.h>
#include <fs/vfs/vfs.h>

extern VFS_FS *root_fs;

/* Static ATA device descriptions for the two legacy IDE channels. */
static ATA_Device ata_devices[2] = {
    {.bus = 0, .device = 0, .io_base = 0x1F0, .ctrl_base = 0x3F6},
    {.bus = 1, .device = 0, .io_base = 0x170, .ctrl_base = 0x376},
};

/* VFS device wrappers around the raw ATA devices. */
static VFS_Device devi[2] = {
    {.device = &ata_devices[0],
     .read = &ata_read_sector,
     .write = &ata_write_sector},
};

/* Partition table storage. */
static gpt_partition_t partitions[20] = {
    {.device = &devi[0]},
};

void init_filesystems(void) {
  printk(KERN_DEBUG "GPT init...\n");
  int gpt_result = gpt_init(partitions);
  if (gpt_result < 0) {
    printk(KERN_ERR "GPT initialization failed: %d\n", gpt_result);
    printk(KERN_WARNING "Continuing without GPT...\n");
  } else {
    printk(KERN_OK "GPT initialized with %d partitions\n", gpt_result);
  }

  /* Ensure partition device callbacks are set. */
  for (int i = 1; i < 2; i++) {
    if (!partitions[i].device->read) {
      partitions[i].device->read = &ata_read_sector;
      partitions[i].device->write = &ata_write_sector;
      partitions[i].device->device = &ata_devices[0];
    }
  }

  printk(KERN_DEBUG "Mounting FAT32 at LBA %d...\n", partitions[0].first_lba);
  vfs_mount("/", &partitions[0], FS_FAT32);
  vfs_mount("/dev", NULL, FS_DEV);
  vfs_mount("/pipe", NULL, FS_PIPE);
  printk(KERN_INFO "Filesystem mounted\n");
}

static int init_filesystems_call(void) {
  init_filesystems();
  return 0;
}

fs_initcall(init_filesystems_call);
