#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

typedef struct __attribute__((packed))
{
	uint16_t bytes_per_sector;		// 0x0B, 2 байти
	uint8_t sectors_per_cluster;	// 0x0D, 1 байт
	uint16_t reserved_sector_count; // 0x0E, 2 байти
	uint8_t num_fats;				// 0x10, 1 байт
	uint16_t root_entry_count;		// 0x11, 2 байти (для FAT12/16)
	uint16_t total_sectors_16;		// 0x13, 2 байти
	uint8_t media;					// 0x15, 1 байт
	uint16_t fat_size_16;			// 0x16, 2 байти
	uint16_t sectors_per_track;		// 0x18, 2 байти
	uint16_t num_heads;				// 0x1A, 2 байти
	uint32_t hidden_sectors;		// 0x1C, 4 байти
	uint32_t total_sectors_32;		// 0x20, 4 байти
	uint32_t fat_size_32;			// 0x24, 4 байти
	uint16_t ext_flags;				// 0x28, 2 байти
	uint16_t fs_version;			// 0x2A, 2 байти
	uint32_t root_cluster;			// 0x2C, 4 байти
	uint16_t fs_info;				// 0x30, 2 байти
	uint16_t backup_boot_sector;	// 0x32, 2 байти
	uint8_t reserved[12];			// 0x34, 12 байт
	uint8_t drive_number;			// 0x40, 1 байт
	uint8_t reserved1;				// 0x41, 1 байт
	uint8_t boot_signature;			// 0x42, 1 байт
	uint32_t volume_id;				// 0x43, 4 байти
	uint8_t volume_label[11];		// 0x47, 11 байт
	uint8_t fs_type[8];				// 0x52, 8 байт
} FAT32_BPB;

typedef struct __attribute__((packed))
{
	uint8_t name[11];
	uint8_t attr;
	uint8_t nt_reserved;
	uint8_t creation_time_tenths;
	uint16_t creation_time;
	uint16_t creation_date;
	uint16_t last_access_date;
	uint16_t first_cluster_high;
	uint16_t write_time;
	uint16_t write_date;
	uint16_t first_cluster_low;
	uint32_t file_size;
} FAT32_DirectoryEntry;
typedef struct
{
	// === Абстракція пристрою ===
	void *device; // будь-який носій (ATA_Device, RamDisk, USB і т.д.)
	int (*read_sector)(void *device, uint32_t lba, void *buffer);
	int (*write_sector)(void *device, uint32_t lba, const void *buffer);

	// === Метадані розділу ===
	uint32_t start_lba;		// початок розділу FAT32
	uint32_t total_sectors; // загальна кількість секторів
	uint32_t sectors_per_cluster;
	uint32_t cluster_size;
	uint32_t total_fat_entries;
	uint32_t bytes_per_sector;

	// FAT32 BPB
	uint32_t reserved_sectors;
	uint32_t num_fats;
	uint32_t sectors_per_fat;
	uint32_t root_cluster;
	uint32_t cluster_heap_lba;
	uint32_t fat_size_32;

	// === FAT cache ===
	uint32_t fat_start_lba;	 // LBA початку FAT
	uint32_t data_start_lba; // LBA початку даних
	uint32_t *fat_cache;	 // кешована FAT (опційно)
	bool fat_dirty;			 // чи треба скидати зміни назад на диск
	bool cache_enabled;

	// === Стан ===
	bool mounted;
} FAT32_FS;

typedef struct
{
	FAT32_DirectoryEntry *entry;
	uint32_t cluster;
	uint32_t index;
} FAT32_File;

#define MAX_PARTS 16
typedef struct
{
	char sfn[12]; // 8.3 формат (11 + \0)
	char *lfn;	  // оригінальне ім’я (якщо треба для LFN)
} PathPart;

typedef struct
{
	int count;
	PathPart parts[MAX_PARTS];
} PathParts;

// extern uint32_t fat32_partition_base_lba;

#define MAX_CLUSTER_CHAIN 1024

// extern uint32_t *fat_cache;
// extern bool fat_dirty;

// extern uint32_t fat_start_lba, cluster_heap_lba, root_cluster;
//  extern uint32_t cluster_size;
//  extern FAT32_BPB *bpb;
//  extern uint32_t total_fat_entries;
