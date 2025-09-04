#pragma once

#include <stdint.h>
#include <utils/vfs/vfs.h>

typedef struct __attribute__((packed))
{
    uint8_t partition_type_guid[16];
    uint8_t unique_partition_guid[16];
    uint64_t first_lba;
    uint64_t last_lba;
    uint64_t attributes;
    uint16_t name[36]; // UTF-16LE
} GPT_Partition_Entry;

typedef struct __attribute__((packed))
{
    uint64_t signature;
    uint32_t revision;
    uint32_t header_size;
    uint32_t header_crc32;
    uint32_t reserved;
    uint64_t current_lba;
    uint64_t backup_lba;
    uint64_t first_usable_lba;
    uint64_t last_usable_lba;
    uint8_t disk_guid[16];
    uint64_t partition_entries_lba;
    uint32_t num_partition_entries;
    uint32_t sizeof_partition_entry;
    uint32_t partition_entries_crc32;
    uint8_t reserved2[420]; // 512 - 92 = 420
} GPT_Header;

typedef struct
{
    uint64_t first_lba;
    uint64_t last_lba;
    char name[37];
    void *device;
    DeviceType type;
} gpt_partition_t;
