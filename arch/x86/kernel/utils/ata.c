#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "utils/ata.h"
#include "utils/io.h"

#define ATA_PRIMARY_IO 0x1F0
#define ATA_PRIMARY_CTRL 0x3F6
#define ATA_STATUS_BSY 0x80
#define ATA_STATUS_DRQ 0x08
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
void ata_wait()
{
    while (inb(ATA_PRIMARY_IO + 7) & ATA_STATUS_BSY)
        ;
}

// DRQ wait
int ata_wait_drq()
{
    uint8_t status;
    do
    {
        status = inb(ATA_PRIMARY_IO + 7);
        if (status & ATA_STATUS_ERROR)
            return -1;
    } while (!(status & ATA_STATUS_DRQ));
    return 0;
}

// Read sector
void ata_read_sector(uint32_t lba, uint8_t *buffer)
{

    ata_wait();

    outb(ATA_PRIMARY_CTRL, 0x00); // disable IRQ (polling mode)

    outb(ATA_PRIMARY_IO + 1, 0x00);                        // null LBA high
    outb(ATA_PRIMARY_IO + 2, 1);                           // sector count = 1
    outb(ATA_PRIMARY_IO + 3, (uint8_t)(lba));              // LBA low
    outb(ATA_PRIMARY_IO + 4, (uint8_t)(lba >> 8));         // LBA mid
    outb(ATA_PRIMARY_IO + 5, (uint8_t)(lba >> 16));        // LBA high
    outb(ATA_PRIMARY_IO + 6, 0xE0 | ((lba >> 24) & 0x0F)); // 0xE0: master + LBA mode
    outb(ATA_PRIMARY_IO + 7, ATA_CMD_READ_SECT);           // команда читання

    ata_wait();
    ata_wait_drq();

    // Зчитування 256 слів (512 байт)
    for (int i = 0; i < 256; i++)
    {
        uint16_t data = inw(ATA_PRIMARY_IO);
        buffer[i * 2] = data & 0xFF;
        buffer[i * 2 + 1] = (data >> 8) & 0xFF;
    }
}

int ata_write_sector(uint32_t lba, const void *buffer)
{
    const uint16_t *data = (const uint16_t *)buffer;

    ata_wait();

    outb(ATA_PRIMARY_IO + 6, 0xE0 | ((lba >> 24) & 0x0F)); // Drive/Head
    outb(ATA_PRIMARY_IO + 2, 1);                           // Sector count
    outb(ATA_PRIMARY_IO + 3, lba & 0xFF);                  // LBA low
    outb(ATA_PRIMARY_IO + 4, (lba >> 8) & 0xFF);           // LBA mid
    outb(ATA_PRIMARY_IO + 5, (lba >> 16) & 0xFF);          // LBA high
    outb(ATA_PRIMARY_IO + 7, ATA_WRITE_SECTORS);           // Command

    if (ata_wait_drq() != 0)
        return -1;

    for (int i = 0; i < 256; i++)
    {
        outw(ATA_PRIMARY_IO, data[i]);
    }

    // Flush cache
    outb(ATA_PRIMARY_IO + 7, 0xE7); // FLUSH CACHE
    ata_wait();

    return 0;
}

void ata_manual_test()
{
    const uint32_t test_lba = 100;
    uint8_t write_buf[512];
    uint8_t read_buf[512];

    // 1. Підготовка тестових даних
    for (int i = 0; i < 512; ++i)
        write_buf[i] = (uint8_t)(i & 0xFF); // просто шаблон: 00, 01, ..., FF, 00, 01 ...

    printf("📤 Writing to LBA %u...\n", test_lba);
    if (ata_write_sector(test_lba, write_buf) != 0)
    {
        printf("ATA write failed\n");
        printf("error code: %d\n", ata_write_sector(test_lba, write_buf));
        return;
    }

    // 2. Обнулити буфер і прочитати назад
    memset(read_buf, 0, sizeof(read_buf));
    printf("Reading from LBA %u...\n", test_lba);
    ata_read_sector(test_lba, read_buf);

    // 3. Порівняння
    for (int i = 0; i < 16; ++i) // перевіримо перші 16 байт
    {
        printf("Byte %02d: written=0x%d read=0x%d\n", i, write_buf[i], read_buf[i]);
    }

    if (memcmp(write_buf, read_buf, 512) == 0)
        printf("ATA write/read successful!\n");
    else
        printf("Mismatch in data!\n");
}
