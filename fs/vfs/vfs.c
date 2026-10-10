/* -- Virtual Filesystem (VFS) core implementation -----------------
 * Implements mount management, path resolution across mountpoints,
 * and the public VFS API (open, read, write, mkdir, readdir, etc.).
 * ------------------------------------------------------------------ */

#include "vfs.h"
#include "vfs_standart_struct.h"

#include <fs/gpt/gpt.h>
#include <hubble/errno.h>
#include <hubble/printk.h>
#include <hubble/string.h>
#include <mm/kmalloc.h>
#include <stdarg.h>

static VFS_Mount *vfs_mounts = NULL;

/** @brief Get the relative path portion after a mountpoint prefix. */
static const char *vfs_get_relpath(const char *path, const char *mountpoint) {
  size_t mlen = strlen(mountpoint);

  if (strcmp(path, mountpoint) == 0)
    return "/";

  if (strncmp(path, mountpoint, mlen) == 0) {
    const char *rel = path + mlen;
    if (*rel == '/')
      rel++;
    return rel;
  }

  return NULL;
}

VFS_Mount *vfs_get_mounts(void) { return vfs_mounts; }

/** @brief Mount a filesystem at the given mountpoint. */
bool vfs_mount(const char *mountpoint, gpt_partition_t *partition,
               FileSystemType type) {
  VFS_FS *fs = kmalloc(sizeof(VFS_FS), GFP_KERNEL);
  memset(fs, 0, sizeof(VFS_FS));
  fs->type = type;

  switch (type) {
  case FS_FAT32:
    printk(KERN_INFO "VFS: init fat32 vfs\n");
    extern void fat32_init_vfs(VFS_FS * fs);
    fat32_init_vfs(fs);
    break;
  case FS_EXT2:
    printk(KERN_INFO "VFS: init ext2 vfs\n");
    extern void ext2_init_vfs(VFS_FS * fs);
    ext2_init_vfs(fs);
    break;
  case FS_DEV:
    printk(KERN_INFO "VFS: init dev vfs\n");
    extern void dev_vfs_init(VFS_FS * fs, VFS_Device * device,
                             uint32_t start_lba);
    dev_vfs_init(fs, partition ? partition->device : NULL,
                 partition ? partition->first_lba : 0);
    break;
  case FS_PIPE:
    printk(KERN_INFO "VFS: init pipe vfs\n");
    extern void pipe_vfs_init(VFS_FS * fs, VFS_Device * device,
                              uint32_t start_lba);
    pipe_vfs_init(fs, partition ? partition->device : NULL,
                  partition ? partition->first_lba : 0);
    break;
  case FS_INITRAMFS:
    printk(KERN_INFO "VFS: init initramfs vfs\n");
    extern void initramfs_init_vfs(VFS_FS * fs);
    initramfs_init_vfs(fs);
    break;
  default:
    printk(KERN_INFO "VFS: unsupported FS type %d\n", type);
    kfree(fs);
    return false;
  }
  printk(KERN_INFO "VFS: mount %d at %s\n", type, mountpoint);

  if (type != FS_DEV && type != FS_PIPE && type != FS_INITRAMFS) {
    if (!partition || !partition->device) {
      printk(KERN_ERR "VFS: no partition for %d at %s\n", type, mountpoint);
      kfree(fs);
      return false;
    }
    if (!fs->mount(fs, partition->device, partition->first_lba)) {
      printk(KERN_ERR "VFS: failed to mount %d at %s\n", type, mountpoint);
      return false;
    }
  }
  return vfs_mount_fs(mountpoint, fs);
}

bool vfs_mount_fs(const char *mountpoint, VFS_FS *fs) {
  if (!mountpoint || !fs ||
      strlen(mountpoint) >= sizeof(((VFS_Mount *)0)->mountpoint))
    return false;

  if (vfs_mounts) {
    VFS_File *dir = vfs_open(mountpoint, VFS_O_RDONLY);
    if (IS_ERR(dir)) {
      printk(KERN_ERR "VFS: mountpoint %s does not exist\n", mountpoint);
      return false;
    }
    bool is_dir = dir->node->is_dir;
    vfs_close(dir);
    if (!is_dir) {
      printk(KERN_ERR "VFS: mountpoint %s is not a directory\n", mountpoint);
      return false;
    }
  }

  VFS_Mount *mnt = kmalloc(sizeof(VFS_Mount), GFP_KERNEL);
  if (!mnt)
    return false;
  strcpy(mnt->mountpoint, mountpoint);
  mnt->fs = fs;
  mnt->next = vfs_mounts;
  vfs_mounts = mnt;

  printk(KERN_INFO "VFS: mounted %d at %s\n", fs->type, mountpoint);
  return true;
}

/** @brief Find the deepest mountpoint matching a path. */
static VFS_Mount *vfs_find_mount_for_path(const char *path) {
  VFS_Mount *best = NULL;
  size_t best_len = 0;
  printk(KERN_DEBUG "vfs_find_mount_for_path: path=%s\n", path);
  for (VFS_Mount *m = vfs_mounts; m; m = m->next) {
    size_t len = strlen(m->mountpoint);

    bool boundary = path[len] == '\0' || path[len] == '/' ||
                    (len > 0 && m->mountpoint[len - 1] == '/');

    if (strncmp(path, m->mountpoint, len) == 0 && boundary) {
      if (len > best_len) {
        best = m;
        best_len = len;
      }
    }
  }

  return best;
}

/* Mount for @p path plus the path inside it (no leading '/'). */
static VFS_Mount *vfs_resolve(const char *path, const char **rel) {
  VFS_Mount *mnt = vfs_find_mount_for_path(path);
  if (!mnt || !mnt->fs)
    return NULL;
  const char *r = vfs_get_relpath(path, mnt->mountpoint);
  while (*r == '/')
    r++;
  *rel = r;
  return mnt;
}

/* Release a node that never got a VFS_File around it. */
static void vfs_drop_node(VFS_FS *fs, VFS_Node *node) {
  if (fs->close) {
    VFS_File tmp = {.node = node};
    fs->close(&tmp);
  }
}

/**
 * @brief Open a file by path with the given VFS_O_* flags.
 *
 * VFS_O_CREAT creates a missing file (-EROFS if the filesystem cannot),
 * with VFS_O_EXCL failing (-EEXIST) if it already exists. VFS_O_TRUNC
 * empties a file opened for writing.
 *
 * @return The open file, or ERR_PTR(negative errno)
 */
VFS_File *vfs_open(const char *path, int flags) {
  const char *rel;
  VFS_Mount *mnt = vfs_resolve(path, &rel);
  if (!mnt)
    return ERR_PTR(-ENOENT);
  VFS_FS *fs = mnt->fs;

  VFS_Node *node = fs->open ? fs->open(fs, rel) : NULL;

  if (node && (flags & VFS_O_CREAT) && (flags & VFS_O_EXCL)) {
    vfs_drop_node(fs, node);
    return ERR_PTR(-EEXIST);
  }
  if (!node) {
    if (!(flags & VFS_O_CREAT))
      return ERR_PTR(-ENOENT);
    if (!fs->create_file)
      return ERR_PTR(-EROFS);
    node = fs->create_file(fs, rel);
    if (!node)
      return ERR_PTR(-ENOENT); /* e.g. the parent directory is missing */
  }

  /* The path is kept inline after the struct so it has exactly the
   * lifetime of the VFS_File with no separate free. */
  size_t path_len = strlen(path) + 1;
  VFS_File *f = kmalloc(sizeof(VFS_File) + path_len, GFP_KERNEL);
  if (!f) {
    vfs_drop_node(fs, node);
    return ERR_PTR(-ENOMEM);
  }
  memcpy(f + 1, path, path_len);
  f->node = node;
  f->flags = flags;
  f->pos = 0;
  f->path = (const char *)(f + 1);

  if ((flags & VFS_O_TRUNC) && (flags & VFS_O_WRONLY) && !node->is_dir &&
      node->size) {
    int err = vfs_truncate(f, 0);
    if (err < 0) {
      vfs_close(f);
      return ERR_PTR(err);
    }
  }
  return f;
}

/** @brief Read from an open VFS file. */
int vfs_read(VFS_File *file, void *buf, uint32_t size) {
  if (!file || !file->node->fs || !file->node->fs->read)
    return -EIO;
  if (file->pos >= file->node->size)
    return 0;
  return file->node->fs->read(file, buf, size);
}

/** @brief Write to an open VFS file (at the end with VFS_O_APPEND). */
int vfs_write(VFS_File *file, const void *buf, uint32_t size) {
  if (!file || !file->node->fs)
    return -EIO;
  /* VFS_O_WRONLY's bit is set for both write-only and read-write. */
  if (!(file->flags & VFS_O_WRONLY))
    return -EBADF;
  if (file->node->is_dir)
    return -EISDIR;
  if (!file->node->fs->write)
    return -EROFS;
  if (file->flags & VFS_O_APPEND)
    file->pos = file->node->size;
  return file->node->fs->write(file, buf, size);
}

int vfs_truncate(VFS_File *file, uint32_t size) {
  if (!file || !file->node || !file->node->fs)
    return -EIO;
  if (file->node->is_dir)
    return -EISDIR;
  if (!file->node->fs->truncate)
    return -EROFS;
  return file->node->fs->truncate(file, size);
}

/** @brief Create a file (returns VFS_Node). */
VFS_Node *vfs_create_file(const char *path) {
  const char *rel;
  VFS_Mount *mnt = vfs_resolve(path, &rel);
  if (!mnt || !mnt->fs->create_file)
    return NULL;
  return mnt->fs->create_file(mnt->fs, rel);
}

/** @brief Create a directory. */
bool vfs_mkdir(const char *path) {
  const char *rel;
  VFS_Mount *mnt = vfs_resolve(path, &rel);
  if (!mnt || !mnt->fs->mkdir)
    return false;
  return mnt->fs->mkdir(mnt->fs, rel);
}

/** @brief Read a directory listing. */
Directory vfs_readdir(const char *path) {
  VFS_Mount *mnt = vfs_find_mount_for_path(path);
  if (!mnt)
    return (Directory){0};
  const char *relpath = vfs_get_relpath(path, mnt->mountpoint);
  if (!mnt->fs->readdir)
    return (Directory){0};
  return mnt->fs->readdir(mnt->fs, relpath);
}

/** @brief Unlink (delete) a file or an empty directory. */
bool vfs_unlink(const char *path) {
  const char *rel;
  VFS_Mount *mnt = vfs_resolve(path, &rel);
  if (!mnt || !mnt->fs->unlink)
    return false;
  return mnt->fs->unlink(mnt->fs, rel);
}

int vfs_rename(const char *from, const char *to) {
  const char *rel_from, *rel_to;
  VFS_Mount *a = vfs_resolve(from, &rel_from);
  VFS_Mount *b = vfs_resolve(to, &rel_to);
  if (!a || !b)
    return -ENOENT;
  if (a != b)
    return -EXDEV;
  if (!a->fs->rename)
    return -EROFS;
  return a->fs->rename(a->fs, rel_from, rel_to) ? 0 : -ENOENT;
}

/** @brief Seek to a position in an open file. */
int vfs_lseek(VFS_File *file, int offset, int whence) {
  if (!file || !file->node->fs)
    return -1;

  switch (whence) {
  case SEEK_SET:
    file->pos = offset;
    break;
  case SEEK_CUR:
    file->pos += offset;
    break;
  case SEEK_END:
    file->pos = file->node->size + offset;
    break;
  default:
    return -EINVAL;
  }

  /* lseek returns the new offset (callers use SEEK_END to get sizes). */
  return (int)file->pos;
}

/** @brief Close an open file and free allocated resources. */
int vfs_close(VFS_File *file) {
  if (!file || !file->node || !file->node->fs || !file->node->fs->close)
    return -EIO;

  file->node->fs->close(file);

  return 0;
}
