/* -- EXT2 driver API ----------------------------------------------
 * The same shape as the FAT32 driver (fs/fat32/fat32.h), so a VFS glue
 * file (ext2_vfs.c) looks the same for both:
 *
 *   mount / unmount        attach to a partition, release it
 *   read_block/write_block one allocation unit (an ext2 block)
 *   alloc_block/free_block take or give back a unit
 *   lookup                 path -> node (paths relative to the root)
 *   readdir                a directory node's entries
 *   read / write           a node's data at a byte position
 *   create / remove        add or delete a path (FAT32 only so far)
 *
 * Errors are negative errno values; success is 0, or a byte count for
 * read and write.
 * ------------------------------------------------------------------ */

#pragma once

#include <fs/ext2/ext2_struct.h>
#include <fs/vfs/vfs_standart_struct.h>
#include <stddef.h>
#include <stdint.h>

/* -- Common driver API ---------------------------------------------- */

/** @brief Read the superblock and group descriptors of the partition at
 * @p first_lba on @p dev. */
int ext2_mount(EXT2_FS *fs, const VFS_Device *dev, uint32_t first_lba);

/** @brief Release what ext2_mount allocated (not @p fs itself). */
void ext2_unmount(EXT2_FS *fs);

/** @brief Read or write block @p block (fs->block_size bytes). */
int ext2_read_block(EXT2_FS *fs, uint32_t block, void *buf);
int ext2_write_block(EXT2_FS *fs, uint32_t block, const void *buf);

/** @brief Allocate a block, preferably in the group of inode @p hint.
 * @return The block, or 0 when the disk is full */
uint32_t ext2_alloc_block(EXT2_FS *fs, uint32_t hint);
int ext2_free_block(EXT2_FS *fs, uint32_t block);

/** @brief Find @p path (following symlinks) and read its inode. */
int ext2_lookup(EXT2_FS *fs, const char *path, EXT2_Node *out);

/** @brief The entries of directory @p dir. */
Directory ext2_readdir(EXT2_FS *fs, const EXT2_Node *dir);

/** @brief Read up to @p size bytes at @p pos. @return Bytes read */
int ext2_read(EXT2_FS *fs, EXT2_Node *node, uint32_t pos, void *buf,
              uint32_t size);

/** @brief Write @p size bytes at @p pos, growing the file. Only the 12
 * direct blocks can be written so far. @return Bytes written */
int ext2_write(EXT2_FS *fs, EXT2_Node *node, uint32_t pos, const void *buf,
               uint32_t size);

/* -- EXT2 specifics --------------------------------------------------- */

/** @brief Read or write inode @p ino. */
int ext2_read_inode(EXT2_FS *fs, uint32_t ino, Ext2Inode *out);
int ext2_write_inode(EXT2_FS *fs, uint32_t ino, const Ext2Inode *inode);

/** @brief Map block @p index of a file to a disk block (0 = hole). */
uint32_t ext2_bmap(EXT2_FS *fs, const Ext2Inode *inode, uint32_t index);

/** @brief Create an empty file @p name in directory @p parent_ino; only
 * the directory's first block is used, so the VFS does not call it yet.
 * @return The new inode number, or 0 */
uint32_t ext2_create_file(EXT2_FS *fs, uint32_t parent_ino, const char *name);

/** @brief Glue for vfs_mount (ext2_vfs.c). */
struct VFS_FS;
void ext2_init_vfs(struct VFS_FS *fs);
