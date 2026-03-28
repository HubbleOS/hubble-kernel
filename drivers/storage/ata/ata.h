#pragma once

#include <stdint.h>

typedef struct
{
	uint8_t bus;	    // 0 = primary, 1 = secondary
	uint8_t device;	    // 0 = master, 1 = slave
	uint16_t io_base;   // base port 0x1F0 or 0x170
	uint16_t ctrl_base; // control port (0x3F6 або 0x376)
} ATA_Device;

int ata_read_sector(void *device, uint32_t lba, void *buffer);
int ata_write_sector(void *device, uint32_t lba, const void *buffer);
void ata_manual_test();
