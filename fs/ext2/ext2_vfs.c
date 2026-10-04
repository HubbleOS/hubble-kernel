/* -- EXT2 VFS wrapper ---------------------------------------------
 * Glue layer between the VFS dispatch table and the EXT2
 * filesystem implementation. Converts VFS callbacks to EXT2
 * function calls.
 * ------------------------------------------------------------------ */

#include <fs/vfs/vfs.h>
#include <fs/vfs/vfs_standart_struct.h>
#include <hubble/printk.h>

#include <fs/ext2/ext2.h>
#include <fs/ext2/ext2_struct.h>
#include <mm/kmalloc.h>

#include <hubble/string.h>

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
  printk(KERN_OK "EXT2 Superblock OK (magic 0x%x)\n", ext2_fs->sb.s_magic);
  fs->fs = ext2_fs;
  return 1;
}

/** @brief Open wrapper. */
static VFS_Node *ext2_vfs_open(VFS_FS *fs, const char *path) {
  if (!fs->fs) {
    printk(KERN_INFO "fs not mounted\n");
    return NULL;
  }
  EXT2_FS *e_fs = (EXT2_FS *)fs->fs;

  uint32_t inode = ext2_parse_path(e_fs, EXT2_ROOT_INO, path);
  if (inode == 0)
    return NULL;

  Ext2Inode inode_buf;
  if (ext2_read_inode(e_fs, inode, &inode_buf) < 0)
    return NULL;

  EXT2_FILE *file = kmalloc(sizeof(EXT2_FILE), GFP_KERNEL);
  if (!file)
    return NULL;
  file->inode_number = inode;
  file->fs = e_fs;
  memcpy(&file->inode, &inode_buf, sizeof(Ext2Inode));

  VFS_Node *node = kmalloc(sizeof(VFS_Node), GFP_KERNEL);
  if (!node) {
    kfree(file);
    return NULL;
  }
  node->fs = fs;
  /* mode & 0x10 (the old FAT attribute constant) is the group-write
   * permission bit, not the directory type. */
  node->is_dir = ext2_is_dir(inode_buf.mode);
  node->fs_node = file;
  node->size = inode_buf.size;
  node->pos = 0;
  return node;
}

/** @brief Read wrapper. */
int ext2_vfs_read(VFS_File *file, void *buffer, uint32_t size) {
  if (!file)
    return -1;
  memset(buffer, 0, size);

  EXT2_FS *fs = (EXT2_FS *)file->node->fs->fs;
  EXT2_FILE *ext2_file = (EXT2_FILE *)file->node->fs_node;
  Ext2Inode *inode = &ext2_file->inode;

  if (ext2_is_dir(inode->mode)) {
    printk(KERN_ERR "cannot read directory\n");
    return -1;
  }

  size_t file_size = inode->size;
  if (file->pos >= file_size)
    return 0;

  size_t to_read =
      (file->pos + size > file_size) ? (file_size - file->pos) : size;
  uint8_t *block_buf = kmalloc(fs->block_size, GFP_KERNEL);
  if (!block_buf) {
    printk(KERN_ERR "failed to allocate block buffer\n");
    return -1;
  }

  size_t read = 0;
  while (read < to_read) {
    uint32_t abs_pos = file->pos + read;
    uint32_t block_index = abs_pos / fs->block_size;
    uint32_t offset_in_block = abs_pos % fs->block_size;

    size_t space_in_block = fs->block_size - offset_in_block;
    size_t remaining = to_read - read;
    size_t chunk = (remaining < space_in_block) ? remaining : space_in_block;

    /* A hole (block 0) reads as zeros; buffer was cleared above. */
    uint32_t block_num = ext2_bmap(fs, inode, block_index);
    if (block_num) {
      ext2_read_block(fs, block_num, block_buf);
      memcpy((uint8_t *)buffer + read, block_buf + offset_in_block, chunk);
    }

    read += chunk;
  }

  kfree(block_buf);
  file->pos += read;
  return (int)read;
}

/** @brief Write wrapper. */
int ext2_vfs_write(VFS_File *file, const void *buffer, uint32_t size) {
  if (!file)
    return -1;

  EXT2_FS *fs = (EXT2_FS *)file->node->fs->fs;
  EXT2_FILE *ext2_file = (EXT2_FILE *)file->node->fs_node;
  Ext2Inode *inode = &ext2_file->inode;

  if (ext2_is_dir(inode->mode)) {
    printk(KERN_ERR "cannot write directory\n");
    return -1;
  }

  uint8_t *block_buf = kmalloc(fs->block_size, GFP_KERNEL);
  if (!block_buf) {
    printk(KERN_ERR "failed to allocate block buffer\n");
    return -1;
  }

  size_t to_write = size;
  size_t written = 0;
  int inode_dirty = 0;

  while (written < to_write) {
    uint32_t abs_pos = file->pos + written;
    uint32_t block_index = abs_pos / fs->block_size;
    uint32_t offset_in_block = abs_pos % fs->block_size;

    if (block_index >= 12) {
      printk(KERN_ERR "indirect blocks not supported\n");
      break;
    }

    size_t space_in_block = fs->block_size - offset_in_block;
    size_t remaining = to_write - written;
    size_t chunk = (remaining < space_in_block) ? remaining : space_in_block;

    uint32_t block_num = inode->block[block_index];
    int is_new_block = (block_num == 0);

    if (is_new_block) {
      block_num = ext2_allocate_block(fs, ext2_file->inode_number);
      if (block_num == 0) {
        printk(KERN_ERR "No space left on device\n");
        break;
      }
      inode->block[block_index] = block_num;
      inode->blocks += fs->block_size / 512;
      inode_dirty = 1;
    }

    if (is_new_block) {
      memset(block_buf, 0, fs->block_size);
    } else if (offset_in_block != 0 || chunk < fs->block_size) {
      ext2_read_block(fs, block_num, block_buf);
    }

    memcpy(block_buf + offset_in_block, (const uint8_t *)buffer + written,
           chunk);
    ext2_write_block(fs, block_num, block_buf);

    written += chunk;
  }

  uint32_t new_end = file->pos + written;
  if (new_end > inode->size) {
    inode->size = new_end;
    inode_dirty = 1;
  }

  if (inode_dirty)
    ext2_write_inode(fs, ext2_file->inode_number, inode);

  kfree(block_buf);
  file->pos += written;
  return (int)written;
}

/** @brief Close wrapper . */
int ext2_vfs_close(VFS_File *file) {

  EXT2_FS *fs = (EXT2_FS *)file->node->fs->fs;
  EXT2_FILE *ext2_file = (EXT2_FILE *)file->node->fs_node;
  Ext2Inode *inode = &ext2_file->inode;

  kfree(ext2_file);
  kfree(file->node);

  return 0;
}

/** @brief Lseek wrapper (stub — not yet implemented). */
int ext2_vfs_lseek(VFS_File *node, int offset, int whence) { return 0; }

/** @brief Unlink wrapper (stub — not yet implemented). */
bool ext2_vfs_unlink(VFS_FS *fs, const char *path) { return 0; }

/** @brief Mkdir wrapper (stub — not yet implemented). */
bool ext2_vfs_mkdir(VFS_FS *fs, const char *path) { return 0; }

/** @brief Readdir wrapper: parse path and list directory. */
Directory ext2_vfs_readdir(VFS_FS *fs, const char *path) {
  Ext2Inode inode;
  if (ext2_read_inode(fs->fs, ext2_parse_path(fs->fs, EXT2_ROOT_INO, path),
                      &inode) < 0)
    return Directory_init((Directory){.entries = NULL});
  return ext2_list_dir(fs->fs, &inode);
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
  //   fs->create_file
}
