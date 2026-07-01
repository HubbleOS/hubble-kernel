/**
 * @file ata.c
 * @brief ATA PIO read/write implementation — LBA28 mode
 */
#include "ata.h"
#include <fs/fat32/fat_structs.h>
#include <hubble/printk.h>
#include <hubble/string.h>
#include <io.h>
#include <stdint.h>

/* -- ATA register and command defines --------------------- */

#define ATA_STATUS_BSY 0x80
#define ATA_STATUS_DRQ 0x08
#define ATA_STATUS_ERROR 0x01
#define SECTOR_SIZE 512

#define ATA_CMD_READ_SECT 0x20
#define ATA_WRITE_SECTORS 0x30

#define ATA_DATA 0x1F0
#define ATA_ERROR 0x1F1
#define ATA_SECCOUNT 0x1F2
#define ATA_LBA_LOW 0x1F3
#define ATA_LBA_MID 0x1F4
#define ATA_LBA_HIGH 0x1F5
#define ATA_DRIVE_HEAD 0x1F6
#define ATA_COMMAND 0x1F7
#define ATA_STATUS 0x1F7

/* -- Wait helpers ----------------------------------------- */

static void ata_wait(ATA_Device *dev) {
  while (inb(dev->io_base + 7) & ATA_STATUS_BSY)
    ;
}

static int ata_wait_drq(ATA_Device *dev) {
  uint8_t status;
  do {
    status = inb(dev->io_base + 7);
    if (status & ATA_STATUS_ERROR)
      return -1;
  } while (!(status & ATA_STATUS_DRQ));
  return 0;
}

/* -- Sector read ------------------------------------------ */

int ata_read_sector(void *device, uint32_t lba, void *buffer) {
  ATA_Device *dev = (ATA_Device *)device;

  ata_wait(dev);
  outb(dev->ctrl_base, 0x00);

  outb(dev->io_base + 2, 1);
  outb(dev->io_base + 3, (uint8_t)(lba));
  outb(dev->io_base + 4, (uint8_t)(lba >> 8));
  outb(dev->io_base + 5, (uint8_t)(lba >> 16));
  outb(dev->io_base + 6, 0xE0 | ((lba >> 24) & 0x0F));
  outb(dev->io_base + 7, ATA_CMD_READ_SECT);

  ata_wait(dev);
  if (ata_wait_drq(dev) != 0) {
    printk(KERN_ERR "ata_wait_drq failed\n");
    return -1;
  }

  for (int i = 0; i < SECTOR_SIZE / 2; i++)
    ((uint16_t *)buffer)[i] = inw(dev->io_base);

  return 0;
}

/* -- Sector write ----------------------------------------- */

int ata_write_sector(void *device, uint32_t lba, const void *buffer) {
  ATA_Device *dev = (ATA_Device *)device;
  const uint16_t *buf = (const uint16_t *)buffer;

  printk(KERN_INFO "bus: %d, device: %d, io_base: %d, ctrl_base: %d\n",
         dev->bus, dev->device, dev->io_base, dev->ctrl_base);
  printk(KERN_INFO "\nata_write_sector %d", lba);

  for (int j = 0; j < 16; j++)
    printk(KERN_INFO "%02X ", buf[j]);
  printk(KERN_INFO "\n");

  if (((FAT32_DirectoryEntry *)(buf))->name[0] == 0x00)
    printk(KERN_INFO "ata_write_sector: buffer is empty\n");

  ata_wait(dev);

  outb(dev->io_base + 6, 0xE0 | ((lba >> 24) & 0x0F));
  outb(dev->io_base + 2, 1);
  outb(dev->io_base + 3, lba & 0xFF);
  outb(dev->io_base + 4, (lba >> 8) & 0xFF);
  outb(dev->io_base + 5, (lba >> 16) & 0xFF);
  outb(dev->io_base + 7, ATA_WRITE_SECTORS);

  if (ata_wait_drq(dev) < 0) {
    printk(KERN_ERR "ata_wait_drq failed\n");
    return -1;
  }

  for (int i = 0; i < SECTOR_SIZE / 2; i++)
    outw(dev->io_base, buf[i]);

  ata_wait(dev);

  if (inb(dev->io_base + 7) & ATA_STATUS_ERROR) {
    printk(KERN_ERR "ata_write_sector failed\n");
    return -1;
  }

  return 0;
}

void ata_manual_test(void) {}
