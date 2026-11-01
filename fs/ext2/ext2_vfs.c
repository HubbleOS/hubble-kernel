/* -- EXT2 VFS glue -------------------------------------------------
 * Hooks the driver API (ext2.h) into a VFS_FS. fs/fat32/fat32_vfs.c is
 * the same file for FAT32.
 * ------------------------------------------------------------------ */

#include "ext2.h"

#include <fs/vfs/vfs.h>
#include <hubble/printk.h>
#include <hubble/string.h>
#include <mm/kmalloc.h>

static bool ext2_vfs_mount(VFS_FS *vfs, VFS_Device *dev, uint32_t first_lba) {
  EXT2_FS *fs = kzalloc(sizeof(EXT2_FS));
  if (!fs)
    return false;
  int err = ext2_mount(fs, dev, first_lba);
  if (err < 0) {
    printk(KERN_ERR "EXT2: mount at LBA %u failed (%d)\n", first_lba, err);
    kfree(fs);
    return false;
  }
  vfs->fs = fs;
  return true;
}

static void ext2_vfs_unmount(VFS_FS *vfs) {
  ext2_unmount(vfs->fs);
  kfree(vfs->fs);
  vfs->fs = NULL;
}

/* A VFS node owning @p n (copied). */
static VFS_Node *ext2_vfs_node(VFS_FS *vfs, const char *path,
                               const EXT2_Node *n) {
  VFS_Node *node = kzalloc(sizeof(VFS_Node));
  EXT2_Node *own = kmalloc(sizeof(EXT2_Node), GFP_KERNEL);
  if (!node || !own) {
    kfree(node);
    kfree(own);
    return NULL;
  }
  *own = *n;
  strncpy(node->name, path, sizeof(node->name) - 1);
  node->fs = vfs;
  node->fs_node = own;
  node->is_dir = ext2_is_dir(n->inode.mode);
  node->size = n->inode.size;
  node->mode = node->is_dir ? MODE_DIR : MODE_FILE;
  node->has_perm = true;
  node->perm = n->inode.mode & 07777;
  return node;
}

static VFS_Node *ext2_vfs_open(VFS_FS *vfs, const char *path) {
  EXT2_Node n;
  if (ext2_lookup(vfs->fs, path, &n) < 0)
    return NULL;
  return ext2_vfs_node(vfs, path, &n);
}

static int ext2_vfs_read(VFS_File *file, void *buf, uint32_t size) {
  int n = ext2_read(file->node->fs->fs, file->node->fs_node, file->pos, buf,
                    size);
  if (n > 0)
    file->pos += n;
  return n;
}

static int ext2_vfs_write(VFS_File *file, const void *buf, uint32_t size) {
  EXT2_Node *own = file->node->fs_node;
  int n = ext2_write(file->node->fs->fs, own, file->pos, buf, size);
  if (n > 0)
    file->pos += n;
  file->node->size = own->inode.size;
  return n;
}

static Directory ext2_vfs_readdir(VFS_FS *vfs, const char *path) {
  EXT2_Node dir;
  if (ext2_lookup(vfs->fs, path, &dir) < 0)
    return Directory_init((Directory){.entries = NULL});
  return ext2_readdir(vfs->fs, &dir);
}

static int ext2_vfs_close(VFS_File *file) {
  if (file->node) {
    kfree(file->node->fs_node);
    kfree(file->node);
    file->node = NULL;
  }
  return 0;
}

/* create_file, mkdir and unlink stay NULL until ext2.h has create and
 * remove: the VFS then answers -EROFS. */
void ext2_init_vfs(VFS_FS *fs) {
  fs->mount = ext2_vfs_mount;
  fs->unmount = ext2_vfs_unmount;
  fs->open = ext2_vfs_open;
  fs->read = ext2_vfs_read;
  fs->write = ext2_vfs_write;
  fs->readdir = ext2_vfs_readdir;
  fs->close = ext2_vfs_close;
}
