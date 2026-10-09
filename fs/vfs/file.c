/**
 * @file file.c
 * @brief Open file descriptions: reference counting and the operations of
 *        VFS-backed files
 */
#include "file.h"
#include "dev.h"
#include <hubble/errno.h>
#include <mm/kmalloc.h>

/* -- VFS-backed files ------------------------------------- */

static long vfs_file_read(file_t *file, void *buf, size_t len) {
  if ((file->flags & FILE_O_ACCMODE) == FILE_O_WRONLY)
    return -EBADF;
  return vfs_read(file->vfs, buf, (uint32_t)len);
}

static long vfs_file_write(file_t *file, const void *buf, size_t len) {
  if ((file->flags & FILE_O_ACCMODE) == FILE_O_RDONLY)
    return -EBADF;
  return vfs_write(file->vfs, buf, (uint32_t)len);
}

/* Character devices answer through their readable() hook; regular files
 * are always ready, as on Linux. */
static short vfs_file_poll(file_t *file) {
  VFS_device_reg *dev = file_device(file);
  if (dev && dev->readable)
    return FILE_POLLOUT | (dev->readable() ? FILE_POLLIN : 0);
  return FILE_POLLIN | FILE_POLLOUT;
}

static struct wait_queue *vfs_file_wait_queue(file_t *file) {
  VFS_device_reg *dev = file_device(file);
  return dev ? dev->read_wq : NULL;
}

static void vfs_file_release(file_t *file) { vfs_close(file->vfs); }

static const file_ops_t vfs_file_ops = {
    .read = vfs_file_read,
    .write = vfs_file_write,
    .poll = vfs_file_poll,
    .wait_queue = vfs_file_wait_queue,
    .release = vfs_file_release,
};

/* -- Lifetime --------------------------------------------- */

file_t *file_alloc(const file_ops_t *ops, int flags, void *priv) {
  file_t *file = kzalloc(sizeof(*file));
  if (!file)
    return NULL;
  file->ops = ops;
  file->flags = flags;
  file->priv = priv;
  atomic_set(&file->refs, 1);
  return file;
}

file_t *file_from_vfs(VFS_File *vfs, int flags) {
  file_t *file = file_alloc(&vfs_file_ops, flags, NULL);
  if (file)
    file->vfs = vfs;
  return file;
}

void file_put(file_t *file) {
  if (atomic_dec(&file->refs) != 1)
    return;
  if (file->ops->release)
    file->ops->release(file);
  kfree(file);
}

VFS_device_reg *file_device(file_t *file) {
  if (!file->vfs || !file->vfs->node || !file->vfs->node->fs ||
      file->vfs->node->fs->type != FS_DEV)
    return NULL;
  return (VFS_device_reg *)file->vfs->node->fs_node;
}
