/* -- EXT2 public API ----------------------------------------------
 * High-level interface for EXT2 filesystem operations including
 * initialisation, inode reading, path resolution, and directory
 * listing.
 * ------------------------------------------------------------------ */

#pragma once

#include <fs/ext2/ext2_struct.h>
#include <stddef.h>
#include <stdint.h>

/** @brief Initialise EXT2 filesystem state. */
int ext2_init(EXT2_FS *fs);

/** @brief Resolve a path to an inode number starting from a given inode. */
uint32_t ext2_parse_path(EXT2_FS *fs, uint32_t inode, const char *path);

/** @brief Find a directory entry by name within a given inode. */
uint32_t ext2_find_dir_entry(EXT2_FS *fs, uint32_t inode, const char *name);

/** @brief Create a new file under a parent inode. */
uint32_t ext2_create_file(EXT2_FS *fs, uint32_t parent_inode, const char *name);

/** @brief List contents of a directory inode. */
Directory ext2_list_dir(EXT2_FS *fs, Ext2Inode *dir_inode);

/** @brief Read an inode from disk into memory. */
int ext2_read_inode(EXT2_FS *fs, uint32_t inode_number, Ext2Inode *out_inode);

typedef struct {
  EXT2_FS *fs;
  uint32_t inode_number;
} EXT2_FILE;
