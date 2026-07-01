/* -- FAT32 core operations ----------------------------------------
 * Lists files/directories, manages the FAT table (get/set/allocate/
 * free), and provides the VFS-level read/write implementation.
 * ------------------------------------------------------------------ */

#include "fat.h"
#include "fat_structs.h"
#include "fat_utils.h"
#include <fs/vfs/vfs_standart_struct.h>
#include <hubble/printk.h>
#include <hubble/string.h>
#include <mm/kmalloc.h>
#include <stdbool.h>

/** @brief Callback that populates a Directory listing structure. */
void list_files_callback(const char *name, bool is_dir, Directory *ctx_ptr) {
  size_t namelen = strlen(name);
  size_t need = namelen + 2;
  ctx_ptr->entries[ctx_ptr->count].name = kmalloc(need, GFP_KERNEL);
  if (!ctx_ptr->entries[ctx_ptr->count].name)
    return;
  memset(ctx_ptr->entries[ctx_ptr->count].name, 0, need);
  memcpy(ctx_ptr->entries[ctx_ptr->count].name, name, namelen);
  ctx_ptr->entries[ctx_ptr->count].is_dir = is_dir;
  ctx_ptr->count++;
}

/** @brief List files in a directory given a cluster number. */
Directory fat32_list_files(FAT32_FS *fs, uint32_t cluster) {
  Directory ctx = Directory_init((Directory){
      .entries = kmalloc(1024 * sizeof(Entry), GFP_KERNEL), .count = 0});
  if (!ctx.entries)
    return (Directory){0};

  iterate_directory(fs, cluster, list_files_callback, &ctx);
  return ctx;
}

/** @brief List files by path. */
Directory fat32_list_files_from_path(FAT32_FS *fs, const char *path) {
  if (!path)
    return (Directory){.entries = NULL, .count = 0};

  uint32_t cluster = fat32_resolve_path(fs, path);
  if (cluster == 0)
    return (Directory){.entries = NULL, .count = 0};

  return fat32_list_files(fs, cluster);
}

/** @brief Read a FAT entry from the cache or disk. */
uint32_t get_fat_entry(FAT32_FS *fs, uint32_t cluster) {
  if (cluster >= fs->total_fat_entries)
    return 0x0FFFFFFF;

  if (fs->fat_cache)
    return fs->fat_cache[cluster] & 0x0FFFFFFF;

  uint32_t fat_offset = cluster * 4;
  uint32_t fat_sector = fs->fat_start_lba + (fat_offset / fs->bytes_per_sector);
  uint32_t offset = fat_offset % fs->bytes_per_sector;

  uint8_t *sector = kmalloc(fs->bytes_per_sector, GFP_KERNEL);
  if (!sector)
    return 0x0FFFFFFF;

  fs->read_sector(fs->device, fat_sector, sector);

  uint32_t entry = sector[offset] | (sector[offset + 1] << 8) |
                   (sector[offset + 2] << 16) | (sector[offset + 3] << 24);

  kfree(sector);

  return entry & 0x0FFFFFFF;
}

/** @brief Write a FAT entry value. */
void set_fat_entry(FAT32_FS *fs, uint32_t cluster, uint32_t value) {
  value &= 0x0FFFFFFF;
  if (fs->fat_cache && cluster < fs->total_fat_entries) {
    fs->fat_cache[cluster] = value;
    fs->fat_dirty = true;
    return;
  }
  uint32_t fat_offset = cluster * 4;
  uint32_t fat_sector = fs->fat_start_lba + (fat_offset / fs->bytes_per_sector);
  uint8_t sector[512];
  fs->read_sector(fs->device, fat_sector, sector);
  uint32_t offset = fat_offset % fs->bytes_per_sector;
  *((uint32_t *)(sector + offset)) = value;
  fs->write_sector(fs->device, fat_sector, sector);
}

/** @brief Free (zero) a cluster in the FAT. */
void fat32_free_cluster(FAT32_FS *fs, uint32_t cluster) {
  if (cluster < 2 || cluster >= fs->total_fat_entries)
    return;

  set_fat_entry(fs, cluster, 0x00000000);
}

/** @brief Allocate a new cluster in the FAT (returns cluster number or 0). */
uint32_t fat32_allocate_cluster(FAT32_FS *fs) {
  if (!(fs->fat_cache)) {
    for (uint32_t i = 2; i < fs->total_fat_entries; ++i) {
      if (i == fs->root_cluster)
        continue;
      if (get_fat_entry(fs, i) == 0x00000000) {
        set_fat_entry(fs, i, 0x0FFFFFFF);
        return i;
      }
    }
    return 0;
  }

  for (uint32_t i = 3; i < fs->total_fat_entries; ++i) {
    if ((fs->fat_cache[i] & 0x0FFFFFFF) == 0x00000000) {
      fs->fat_cache[i] = 0x0FFFFFFF;
      fs->fat_dirty = true;
      return i;
    }
  }
  return 0;
}
