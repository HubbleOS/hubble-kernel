/*
 * Filesystem initialisation — initramfs as initial root,
 * then persistent storage discovery.
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
  /* Mount initramfs as the initial root filesystem */
  printk(KERN_INFO "Mounting initramfs at /\n");
  vfs_mount("/", NULL, FS_INITRAMFS);

  /* Mount devfs and pipefs */
  vfs_mount("/dev", NULL, FS_DEV);
  vfs_mount("/pipe", NULL, FS_PIPE);

  /* Discover persistent storage (ATA/GPT/FAT32) */
  printk(KERN_DEBUG "GPT init...\n");

  /* Probe for ATA devices before attempting GPT */
  int detected = 0;
  for (int i = 0; i < 2; i++) {
    if (ata_probe(&ata_devices[i]) == 0) {
      detected++;
    }
  }

  if (detected == 0) {
    printk(KERN_WARNING "No ATA devices found, skipping GPT\n");
  } else {
    int gpt_result = gpt_init(partitions, 20);
    if (gpt_result < 0) {
      printk(KERN_WARNING "GPT initialization failed: %d\n", gpt_result);
    } else {
      printk(KERN_OK "GPT initialized with %d partitions\n", gpt_result);
      printk(KERN_INFO "Persistent storage available at disk partitions\n");
    }
  }

  printk(KERN_INFO "Filesystem initialized\n");
}

static int init_filesystems_call(void) {
  init_filesystems();
  return 0;
}

fs_initcall(init_filesystems_call);
