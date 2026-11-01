/* -- FAT32 driver API ---------------------------------------------
 * The same shape as the EXT2 driver (fs/ext2/ext2.h), so a VFS glue
 * file (fat32_vfs.c) looks the same for both:
 *
 *   mount / unmount        attach to a partition, release it
 *   read_block/write_block one allocation unit (a FAT32 cluster)
 *   alloc_block/free_block take or give back a unit
 *   lookup                 path -> node (paths relative to the root)
 *   readdir                a directory node's entries
 *   read / write           a node's data at a byte position
 *   create / remove        add or delete a path
 *
 * Errors are negative errno values; success is 0, or a byte count for
 * read and write. Names are 8.3 short names (long names are not read or
 * written yet), matched without regard to case.
 * ------------------------------------------------------------------ */

#pragma once

#include <fs/fat32/fat32_struct.h>
#include <fs/vfs/vfs_standart_struct.h>
#include <stdbool.h>
#include <stdint.h>

/* -- Common driver API ---------------------------------------------- */

/** @brief Read the boot sector and the FAT of the partition at
 * @p first_lba on @p dev. */
int fat32_mount(FAT32_FS *fs, const VFS_Device *dev, uint32_t first_lba);

/** @brief Write the FAT back and release it (not @p fs itself). */
void fat32_unmount(FAT32_FS *fs);

/** @brief Read or write cluster @p block (fs->cluster_size bytes). */
int fat32_read_block(FAT32_FS *fs, uint32_t block, void *buf);
int fat32_write_block(FAT32_FS *fs, uint32_t block, const void *buf);

/** @brief Allocate a cluster, searching from @p hint, and end a chain
 * there. @return The cluster, or 0 when the disk is full */
uint32_t fat32_alloc_block(FAT32_FS *fs, uint32_t hint);
int fat32_free_block(FAT32_FS *fs, uint32_t block);

/** @brief Find @p path. */
int fat32_lookup(FAT32_FS *fs, const char *path, FAT32_Node *out);

/** @brief The entries of directory @p dir. */
Directory fat32_readdir(FAT32_FS *fs, const FAT32_Node *dir);

/** @brief Read up to @p size bytes at @p pos. @return Bytes read */
int fat32_read(FAT32_FS *fs, FAT32_Node *node, uint32_t pos, void *buf,
               uint32_t size);

/** @brief Write @p size bytes at @p pos, growing the file.
 * @return Bytes written */
int fat32_write(FAT32_FS *fs, FAT32_Node *node, uint32_t pos,
                const void *buf, uint32_t size);

/** @brief Create an empty file or directory at @p path. */
int fat32_create(FAT32_FS *fs, const char *path, bool is_dir,
                 FAT32_Node *out);

/** @brief Delete the file or empty directory at @p path. */
int fat32_remove(FAT32_FS *fs, const char *path);

/* -- FAT32 specifics ------------------------------------------------- */

/** @brief Write the FAT (every copy) back if it changed. */
int fat32_sync(FAT32_FS *fs);

/** @brief Glue for vfs_mount (fat32_vfs.c). */
struct VFS_FS;
void fat32_init_vfs(struct VFS_FS *fs);
