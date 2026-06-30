/**
 * @file ata.h
 * @brief ATA PIO driver — sector-level read/write interface
 */
#pragma once

#include <stdint.h>

/**
 * @brief ATA device descriptor
 */
typedef struct
{
	uint8_t bus;        /* 0 = primary, 1 = secondary */
	uint8_t device;     /* 0 = master, 1 = slave */
	uint16_t io_base;   /* base port 0x1F0 or 0x170 */
	uint16_t ctrl_base; /* control port 0x3F6 or 0x376 */
} ATA_Device;

/**
 * @brief Read one 512-byte sector from an ATA device
 * @param device  Pointer to an ATA_Device
 * @param lba     Logical Block Address
 * @param buffer  Destination buffer (must be at least 512 bytes)
 * @return 0 on success, -1 on error
 */
int ata_read_sector(void *device, uint32_t lba, void *buffer);

/**
 * @brief Write one 512-byte sector to an ATA device
 * @param device  Pointer to an ATA_Device
 * @param lba     Logical Block Address
 * @param buffer  Source buffer (must be at least 512 bytes)
 * @return 0 on success, -1 on error
 */
int ata_write_sector(void *device, uint32_t lba, const void *buffer);

void ata_manual_test(void);
