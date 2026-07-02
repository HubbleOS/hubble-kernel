/* -- EXT2 VFS wrapper ---------------------------------------------
 * Glue layer between the VFS dispatch table and the EXT2
 * filesystem implementation. Converts VFS callbacks to EXT2
 * function calls.
 * ------------------------------------------------------------------ */

#include "vfs.h"
#include "vfs_standart_struct.h"
#include <hubble/printk.h>

#include <fs/ext2/ext2.h>
#include <fs/ext2/ext2_struct.h>
#include <mm/kmalloc.h>

#include <hubble/string.h>

#define EXT2_ATTR_READ_ONLY 0x01
#define EXT2_ATTR_HIDDEN 0x02
#define EXT2_ATTR_SYSTEM 0x04
#define EXT2_ATTR_VOLUME_ID 0x08
#define EXT2_ATTR_DIRECTORY 0x10
#define EXT2_ATTR_ARCHIVE 0x20

#define EXT2_BLOCK_SIZE 1024

/** @brief Mount wrapper: initialise EXT2 filesystem. */
bool ext2_vfs_init(VFS_FS *fs, VFS_Device *device, uint32_t start_lba) {
  printk(KERN_INFO "Initializing device\n");
  EXT2_FS *ext2_fs = kmalloc(sizeof(EXT2_FS), GFP_KERNEL);
  ext2_fs->device = device->device;
  ext2_fs->read_sector = device->read;
  if (!ext2_fs->read_sector) {
    if (device->read == NULL)
      printk(KERN_ERR "EXT2: read_sector is NULL from param\n");
    printk(KERN_ERR "EXT2: read_sector is NULL\n");
    return 0;
  }
  ext2_fs->write_sector = device->write;
  ext2_fs->first_lba = start_lba;
  printk(KERN_INFO "Initializing EXT2 on partition starting at LBA %u\n",
         start_lba);
  ext2_init(ext2_fs);
  printk(KERN_OK "EXT2 Superblock OK (magic 0x%x)\n", ext2_fs->magic);
  fs->fs = ext2_fs;
  return 1;
}

/** @brief Open wrapper (stub — not yet implemented). */
static VFS_Node *ext2_vfs_open(VFS_FS *fs, const char *path) { return NULL; }

/** @brief Read wrapper (stub — not yet implemented). */
int ext2_vfs_read(VFS_File *node, void *buffer, uint32_t size) { return 0; }

/** @brief Write wrapper (stub — not yet implemented). */
int ext2_vfs_write(VFS_File *node, const void *buffer, uint32_t size) {
  return 0;
}

/** @brief Close wrapper (stub — not yet implemented). */
int ext2_vfs_close(VFS_File *file) { return 0; }

/** @brief Lseek wrapper (stub — not yet implemented). */
int ext2_vfs_lseek(VFS_File *node, int offset, int whence) { return 0; }

/** @brief Unlink wrapper (stub — not yet implemented). */
bool ext2_vfs_unlink(VFS_FS *fs, const char *path) { return 0; }

/** @brief Mkdir wrapper (stub — not yet implemented). */
bool ext2_vfs_mkdir(VFS_FS *fs, const char *path) { return 0; }

/** @brief Readdir wrapper: parse path and list directory. */
Directory ext2_vfs_readdir(VFS_FS *fs, const char *path) {

  printk(KERN_INFO "path: %s\n", path);
  Ext2Inode *inode = kmalloc(sizeof(Ext2Inode), GFP_KERNEL);
  ext2_read_inode(fs->fs, ext2_parse_path(fs->fs, 2, path), inode);
  return ext2_list_dir(fs->fs, inode);
}

/** @brief Initialise VFS dispatch table for EXT2. */
void ext2_init_vfs(VFS_FS *fs) {
  fs->mount = ext2_vfs_init;
  fs->open = ext2_vfs_open;
  fs->read = ext2_vfs_read;
  fs->write = ext2_vfs_write;
  fs->close = ext2_vfs_close;
  fs->unlink = ext2_vfs_unlink;
  fs->mkdir = ext2_vfs_mkdir;
  fs->readdir = ext2_vfs_readdir;
}
