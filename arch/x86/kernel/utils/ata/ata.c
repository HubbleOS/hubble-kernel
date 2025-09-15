#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "utils/ata/ata.h"
#include "utils/io.h"
#include "utils/fat32/fat_structs.h"

#define ATA_STATUS_BSY 0x80
#define ATA_STATUS_DRQ 0x08
#define SECTOR_SIZE 512

#define ATA_CMD_READ_SECT 0x20

#define ATA_DATA 0x1F0
#define ATA_ERROR 0x1F1
#define ATA_SECCOUNT 0x1F2
#define ATA_LBA_LOW 0x1F3
#define ATA_LBA_MID 0x1F4
#define ATA_LBA_HIGH 0x1F5
#define ATA_DRIVE_HEAD 0x1F6
#define ATA_COMMAND 0x1F7
#define ATA_STATUS 0x1F7
#define ATA_STATUS_ERROR 0x01

#define ATA_WRITE_SECTORS 0x30

// Wait busy
static void ata_wait(ATA_Device *dev)
{
	while (inb(dev->io_base + 7) & ATA_STATUS_BSY)
		;
}

// DRQ wait
int ata_wait_drq(ATA_Device *dev)
{
	uint8_t status;
	do
	{
		status = inb(dev->io_base + 7);
		if (status & ATA_STATUS_ERROR)
			return -1;
	} while (!(status & ATA_STATUS_DRQ));
	return 0;
}

// Read sector

int ata_read_sector(void *device, uint32_t lba, void *buffer)
{
	printf("ata_read_sector %d\n", lba);
	ATA_Device *dev = (ATA_Device *)device;
	printf("bus: %d, device: %d, io_base: %d, ctrl_base: %d\n", dev->bus, dev->device, dev->io_base, dev->ctrl_base);

	uint16_t *buf = (uint16_t *)buffer;
	// printf("ata_read_sector");
	// for (int j = 0; j < 16; j++)
	// 	printf("%02X ", buf[j]);
	ata_wait(dev);
	outb(dev->ctrl_base, 0x00);

	outb(dev->io_base + 2, 1); // sector count = 1
	outb(dev->io_base + 3, (uint8_t)(lba));
	outb(dev->io_base + 4, (uint8_t)(lba >> 8));
	outb(dev->io_base + 5, (uint8_t)(lba >> 16));
	outb(dev->io_base + 6, 0xE0 | ((lba >> 24) & 0x0F));
	outb(dev->io_base + 7, ATA_CMD_READ_SECT);

	ata_wait(dev);
	if (ata_wait_drq(dev) != 0)
	{
		printf("ata_wait_drq failed\n");
		return -1;
	}

	for (int i = 0; i < SECTOR_SIZE / 2; i++)
	{
		((uint16_t *)buffer)[i] = inw(dev->io_base);
	}

	return 0;
}

int ata_write_sector(void *device, uint32_t lba, const void *buffer)
{
	ATA_Device *dev = (ATA_Device *)device;
	printf("bus: %d, device: %d, io_base: %d, ctrl_base: %d\n", dev->bus, dev->device, dev->io_base, dev->ctrl_base);
	const uint16_t *buf = (const uint16_t *)buffer;
	printf("\nata_write_sector %d", lba);

	for (int j = 0; j < 16; j++)
		printf("%02X ", buf[j]);
	if (((FAT32_DirectoryEntry *)(buf))->name[0] == 0x00)
	{
		printf("ata_write_sector: buffer is empty\n");
	}
	ata_wait(dev);

	outb(dev->io_base + 6, 0xE0 | ((lba >> 24) & 0x0F)); // Drive/Head
	outb(dev->io_base + 2, 1);			     // Sector count
	outb(dev->io_base + 3, lba & 0xFF);		     // LBA low
	outb(dev->io_base + 4, (lba >> 8) & 0xFF);	     // LBA mid
	outb(dev->io_base + 5, (lba >> 16) & 0xFF);	     // LBA high
	outb(dev->io_base + 7, ATA_WRITE_SECTORS);	     // Command

	if (ata_wait_drq(dev) < 0)
	{
		printf("ata_wait_drq failed\n");
		return -1;
	}

	for (int i = 0; i < SECTOR_SIZE / 2; i++)
	{
		outw(dev->io_base, buf[i]);
	}

	ata_wait(dev);

	if (inb(dev->io_base + 7) & ATA_STATUS_ERROR)
	{
		printf("ata_write_sector failed\n");
		return -1;
	}

	return 0;
}
// void ata_init_device(ATA_Device *dev, uint8_t bus, uint16_t io_base, uint8_t device, uint16_t ctrl_base)
// {
// 	dev->bus = bus;
// 	dev->device = device;
// 	dev->io_base = io_base;
// 	dev->ctrl_base = ctrl_base;
// 	dev->read = &ata_read_sector;
// 	dev->write = &ata_write_sector;
// }

// void ata_manual_test()
// {
//     const uint32_t test_lba = 100;
//     uint8_t write_buf[512];
//     uint8_t read_buf[512];

//     // 1. Підготовка тестових даних
//     for (int i = 0; i < 512; ++i)
//         write_buf[i] = (uint8_t)(i & 0xFF); // просто шаблон: 00, 01, ..., FF, 00, 01 ...

//     printf("📤 Writing to LBA %u...\n", test_lba);
//     if (ata_write_sector(test_lba, write_buf) != 0)
//     {
//         printf("ATA write failed\n");
//         printf("error code: %d\n", ata_write_sector(test_lba, write_buf));
//         return;
//     }

//     memset(read_buf, 0, sizeof(read_buf));
//     printf("Reading from LBA %u...\n", test_lba);
//     ata_read_sector(test_lba, read_buf);

//     // 3. Порівняння
//     for (int i = 0; i < 16; ++i) // перевіримо перші 16 байт
//     {
//         printf("Byte %02d: written=0x%d read=0x%d\n", i, write_buf[i], read_buf[i]);
//     }

//     if (memcmp(write_buf, read_buf, 512) == 0)
//         printf("ATA write/read successful!\n");
//     else
//         printf("Mismatch in data!\n");
// }
