/* -- FAT32 VFS glue ------------------------------------------------
 * Hooks the driver API (fat32.h) into a VFS_FS. fs/ext2/ext2_vfs.c is
 * the same file for EXT2.
 * ------------------------------------------------------------------ */

#include "fat32.h"

#include <fs/vfs/vfs.h>
#include <hubble/printk.h>
#include <hubble/string.h>
#include <mm/kmalloc.h>

static bool fat32_vfs_mount(VFS_FS *vfs, VFS_Device *dev, uint32_t first_lba) {
  FAT32_FS *fs = kzalloc(sizeof(FAT32_FS));
  if (!fs)
    return false;
  int err = fat32_mount(fs, dev, first_lba);
  if (err < 0) {
    printk(KERN_ERR "FAT32: mount at LBA %u failed (%d)\n", first_lba, err);
    kfree(fs);
    return false;
  }
  vfs->fs = fs;
  return true;
}

static void fat32_vfs_unmount(VFS_FS *vfs) {
  fat32_unmount(vfs->fs);
  kfree(vfs->fs);
  vfs->fs = NULL;
}

/* A VFS node owning @p n (copied). */
static VFS_Node *fat32_vfs_node(VFS_FS *vfs, const char *path,
                                const FAT32_Node *n) {
  VFS_Node *node = kzalloc(sizeof(VFS_Node));
  FAT32_Node *own = kmalloc(sizeof(FAT32_Node), GFP_KERNEL);
  if (!node || !own) {
    kfree(node);
    kfree(own);
    return NULL;
  }
  *own = *n;
  strncpy(node->name, path, sizeof(node->name) - 1);
  node->fs = vfs;
  node->fs_node = own;
  node->is_dir = fat32_is_dir(&n->entry);
  node->size = n->entry.file_size;
  node->mode = node->is_dir ? MODE_DIR : MODE_FILE;
  /* FAT has no owners or modes; stat shows the defaults. */
  return node;
}

static VFS_Node *fat32_vfs_open(VFS_FS *vfs, const char *path) {
  FAT32_Node n;
  if (fat32_lookup(vfs->fs, path, &n) < 0)
    return NULL;
  return fat32_vfs_node(vfs, path, &n);
}

static int fat32_vfs_read(VFS_File *file, void *buf, uint32_t size) {
  int n = fat32_read(file->node->fs->fs, file->node->fs_node, file->pos, buf,
                     size);
  if (n > 0)
    file->pos += n;
  return n;
}

static int fat32_vfs_write(VFS_File *file, const void *buf, uint32_t size) {
  FAT32_Node *own = file->node->fs_node;
  int n = fat32_write(file->node->fs->fs, own, file->pos, buf, size);
  if (n > 0)
    file->pos += n;
  file->node->size = own->entry.file_size;
  return n;
}

static Directory fat32_vfs_readdir(VFS_FS *vfs, const char *path) {
  FAT32_Node dir;
  if (fat32_lookup(vfs->fs, path, &dir) < 0)
    return Directory_init((Directory){.entries = NULL});
  return fat32_readdir(vfs->fs, &dir);
}

static int fat32_vfs_close(VFS_File *file) {
  if (file->node) {
    kfree(file->node->fs_node);
    kfree(file->node);
    file->node = NULL;
  }
  return 0;
}

static VFS_Node *fat32_vfs_create_file(VFS_FS *vfs, const char *path) {
  FAT32_Node n;
  if (fat32_create(vfs->fs, path, false, &n) < 0)
    return NULL;
  return fat32_vfs_node(vfs, path, &n);
}

static bool fat32_vfs_mkdir(VFS_FS *vfs, const char *path) {
  FAT32_Node n;
  return fat32_create(vfs->fs, path, true, &n) == 0;
}

static bool fat32_vfs_unlink(VFS_FS *vfs, const char *path) {
  return fat32_remove(vfs->fs, path) == 0;
}

void fat32_init_vfs(VFS_FS *fs) {
  fs->mount = fat32_vfs_mount;
  fs->unmount = fat32_vfs_unmount;
  fs->open = fat32_vfs_open;
  fs->read = fat32_vfs_read;
  fs->write = fat32_vfs_write;
  fs->readdir = fat32_vfs_readdir;
  fs->close = fat32_vfs_close;
  fs->create_file = fat32_vfs_create_file;
  fs->mkdir = fat32_vfs_mkdir;
  fs->unlink = fat32_vfs_unlink;
}
