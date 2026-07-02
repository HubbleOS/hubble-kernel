/* -- FAT32 path resolution and directory iteration ----------------
 * Core functions for resolving paths to clusters, finding directory
 * entries, and iterating over directory contents.
 * ------------------------------------------------------------------ */

#include "fat.h"
#include "fat_structs.h"
#include "fat_utils.h"
#include <hubble/printk.h>
#include <hubble/string.h>
#include <mm/kmalloc.h>

/** @brief Resolve a path to its final cluster (public interface). */
uint32_t fat32_resolve_path(FAT32_FS *fs, const char *path) {
  if (!path || *path == '\0' || strcmp(path, "/") == 0)
    return fs->root_cluster;

  PathParts parts = format_folder_path(path);
  uint32_t cluster = fs->root_cluster;

  for (int i = 0; i < parts.count; i++) {
    cluster = find_directory_entry_cluster(fs, cluster, parts.parts[i].sfn);
    if (cluster == 0 || cluster >= 0x0FFFFFF8) {
      cluster = 0;
      break;
    }
  }

  free_folder_path(&parts);
  return cluster;
}

/** @brief Resolve the parent path to a cluster. */
uint32_t resolve_path_to_cluster(FAT32_FS *fs, const char *path) {
  printk(KERN_INFO "Resolving path to cluster: %s\n", path);
  PathParts parts = format_folder_path(path);
  int depth = parts.count;

  uint32_t cluster = fs->root_cluster;
  for (int i = 0; i < depth - 1; ++i) {
    printk(KERN_INFO "part: %s\n", parts.parts[i].sfn);
    cluster = find_directory_entry_cluster(fs, cluster, parts.parts[i].sfn);
    printk(KERN_INFO "cluster: %d\n", cluster);
    if (cluster == 0 || cluster >= 0x0FFFFFF8) {
      free_folder_path(&parts);
      return 0;
    }
  }

  printk(KERN_INFO "cluster: %d\n", cluster);
  free_folder_path(&parts);
  return cluster;
}

/** @brief Find a directory entry's cluster in a directory chain. */
uint32_t find_directory_entry_cluster(FAT32_FS *fs, uint32_t dir_cluster,
                                      const char *name11) {
  if (!fs || !name11)
    return 0;

  if (dir_cluster == 0)
    dir_cluster = 2;

  uint8_t *buffer = kmalloc(fs->cluster_size, GFP_KERNEL);
  if (!buffer)
    return 0;

  int steps = 0;
  while (dir_cluster < 0x0FFFFFF8 && steps++ < MAX_CLUSTER_CHAIN) {
    fat32_read_cluster(fs, dir_cluster, buffer);
    size_t entries = fs->cluster_size / sizeof(FAT32_DirectoryEntry);

    for (size_t i = 0; i < entries; ++i) {
      FAT32_DirectoryEntry *entry =
          (FAT32_DirectoryEntry *)(buffer + i * sizeof(FAT32_DirectoryEntry));

      if ((entry->attr & 0x0F) == 0x0F || entry->name[0] == 0x00 ||
          entry->name[0] == 0xE5)
        continue;

      if (memcmp(entry->name, name11, 11) == 0) {
        uint32_t cluster =
            (entry->first_cluster_high << 16) | entry->first_cluster_low;
        kfree(buffer);
        return cluster;
      }
    }

    dir_cluster = get_fat_entry(fs, dir_cluster);
  }

  kfree(buffer);
  return 0;
}

/** @brief Walk a directory cluster chain, calling callback for each entry. */
void iterate_directory(FAT32_FS *fs, uint32_t cluster,
                       directory_entry_callback_t callback, void *ctx) {
  int steps = 0;
  uint8_t *data = kmalloc(fs->cluster_size, GFP_KERNEL);
  if (!data)
    return;

  while (cluster < 0x0FFFFFF8 && steps++ < MAX_CLUSTER_CHAIN && cluster != 0) {
    fat32_read_cluster(fs, cluster, data);
    size_t entries = fs->cluster_size / sizeof(FAT32_DirectoryEntry);

    for (size_t i = 0; i < entries; ++i) {
      FAT32_DirectoryEntry *entry =
          (FAT32_DirectoryEntry *)(data + i * sizeof(FAT32_DirectoryEntry));
      char name[20];
      bool is_dir;
      if (parse_directory_entry(entry, name, &is_dir))
        callback(name, is_dir, ctx);
    }

    cluster = get_fat_entry(fs, cluster);
  }

  kfree(data);
}
