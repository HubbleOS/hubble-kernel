#pragma once

#include <stdint.h>

void ata_read_sector(uint32_t lba, uint8_t *buffer);
int ata_write_sector(uint32_t lba, const void *buffer);
void ata_manual_test();