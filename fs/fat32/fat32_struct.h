/* -- FAT32 data structures -----------------------------------------
 * The BPB and directory entry on disk, and the mounted filesystem's
 * state.
 * ------------------------------------------------------------------ */

#pragma once

#include <fs/vfs/vfs_standart_struct.h>
#include <stdbool.h>
#include <stdint.h>

/** @brief FAT32 BIOS Parameter Block (packed on-disk format). */
typedef struct __attribute__((packed)) {
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
typedef struct __attribute__((packed)) {
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

#define FAT32_ATTR_READ_ONLY 0x01
#define FAT32_ATTR_VOLUME_ID 0x08
#define FAT32_ATTR_DIRECTORY 0x10
#define FAT32_ATTR_ARCHIVE 0x20
#define FAT32_ATTR_LFN 0x0F /* long-name entry (all four low bits) */

#define FAT32_DIRENT_END 0x00  /* name[0]: no entries after this one */
#define FAT32_DIRENT_FREE 0xE5 /* name[0]: deleted */

#define FAT32_EOC 0x0FFFFFF8 /* FAT values from here on end a chain */

/** @brief A mounted FAT32 filesystem. Its "blocks" are clusters. */
typedef struct {
  VFS_Device dev;
  uint32_t first_lba; /* the partition's first sector */

  uint32_t sectors_per_cluster;
  uint32_t cluster_size; /* bytes */
  uint32_t cluster_count;
  uint32_t root_cluster;

  uint32_t fat_lba;     /* first FAT */
  uint32_t fat_sectors; /* one FAT */
  uint32_t num_fats;
  uint32_t data_lba; /* cluster 2 */

  uint32_t *fat; /* the whole FAT, written back by fat32_sync */
  uint32_t dirty_lo, dirty_hi; /* changed FAT sectors [lo, hi) */
} FAT32_FS;

/** @brief A file or directory: its directory entry and where that entry
 * is on disk (the root directory has no entry of its own). */
typedef struct {
  FAT32_DirectoryEntry entry;
  uint32_t dir_cluster; /* cluster holding the entry */
  uint32_t index;       /* entry number in that cluster */
  bool is_root;
} FAT32_Node;

static inline uint32_t fat32_first_cluster(const FAT32_DirectoryEntry *e) {
  return (uint32_t)e->first_cluster_high << 16 | e->first_cluster_low;
}

static inline bool fat32_is_dir(const FAT32_DirectoryEntry *e) {
  return e->attr & FAT32_ATTR_DIRECTORY;
}
