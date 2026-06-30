/* ── FAT32 data structures ─────────────────────────────────────────
 * Core on-disk and in-memory structures for the FAT32 filesystem:
 * BPB, directory entry, filesystem state, path parsing helpers.
 * ────────────────────────────────────────────────────────────────── */

#pragma once

#include <stdint.h>
#include <stdbool.h>

/** @brief FAT32 BIOS Parameter Block (packed on-disk format). */
typedef struct __attribute__((packed))
{
	uint16_t bytes_per_sector;
	uint8_t sectors_per_cluster;
	uint16_t reserved_sector_count;
	uint8_t num_fats;
	uint16_t root_entry_count;
	uint16_t total_sectors_16;
	uint8_t media;
	uint16_t fat_size_16;
	uint16_t sectors_per_track;
	uint16_t num_heads;
	uint32_t hidden_sectors;
	uint32_t total_sectors_32;
	uint32_t fat_size_32;
	uint16_t ext_flags;
	uint16_t fs_version;
	uint32_t root_cluster;
	uint16_t fs_info;
	uint16_t backup_boot_sector;
	uint8_t reserved[12];
	uint8_t drive_number;
	uint8_t reserved1;
	uint8_t boot_signature;
	uint32_t volume_id;
	uint8_t volume_label[11];
	uint8_t fs_type[8];
} FAT32_BPB;

/** @brief FAT32 on-disk directory entry (packed, 32 bytes). */
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

/** @brief In-memory FAT32 filesystem state. */
typedef struct
{
	void *device;
	int (*read_sector)(void *device, uint32_t lba, void *buffer);
	int (*write_sector)(void *device, uint32_t lba, const void *buffer);

	uint32_t start_lba;
	uint32_t total_sectors;
	uint32_t sectors_per_cluster;
	uint32_t cluster_size;
	uint32_t total_fat_entries;
	uint32_t bytes_per_sector;

	uint32_t reserved_sectors;
	uint32_t num_fats;
	uint32_t sectors_per_fat;
	uint32_t root_cluster;
	uint32_t cluster_heap_lba;
	uint32_t fat_size_32;

	uint32_t fat_start_lba;
	uint32_t data_start_lba;
	uint32_t *fat_cache;
	bool fat_dirty;
	bool cache_enabled;

	bool mounted;
} FAT32_FS;

/** @brief Open FAT32 file handle. */
typedef struct
{
	FAT32_DirectoryEntry *entry;
	uint32_t cluster;
	uint32_t index;
} FAT32_File;

#define MAX_PARTS 16

/** @brief Single path component (8.3 name + optional LFN). */
typedef struct
{
	char sfn[12];
	char *lfn;
} PathPart;

/** @brief Parsed path broken into components. */
typedef struct
{
	int count;
	PathPart parts[MAX_PARTS];
} PathParts;

#define MAX_CLUSTER_CHAIN 1024
