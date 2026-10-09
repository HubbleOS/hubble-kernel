/*
 * Syscalls that act on paths and file sizes: access, openat, truncate,
 * ftruncate, unlink, rmdir, mkdir, rename, fsync - plus the path helper
 * every path-taking syscall uses.
 */

#include <hubble/errno.h>
#include <hubble/string.h>
#include <hubble/syscalls.h>

#include <fs/vfs/file.h>
#include <fs/vfs/vfs.h>
#include <smp/fdtable.h>
#include <smp/scheduler.h>
#include <smp/task.h>

#include "syscall_entry.h"

#define AT_FDCWD (-100)

const char *sys_abs_path(const char *path, char *buf, size_t size) {
  if (!path)
    return NULL;
  size_t len = strlen(path);
  bool relative = path[0] != '/';
  if (len + (relative ? 1 : 0) + 1 > size)
    return NULL;
  if (relative)
    buf[0] = '/';
  memcpy(buf + (relative ? 1 : 0), path, len + 1);
  return buf;
}

/* Open @p path just to learn whether it exists and what it is. */
static long probe(const char *abs, bool *is_dir) {
  VFS_File *f = vfs_open(abs, VFS_O_RDONLY);
  if (IS_ERR(f))
    return PTR_ERR(f);
  if (!f)
    return -ENOENT;
  if (is_dir)
    *is_dir = f->node->is_dir;
  vfs_close(f);
  return 0;
}

#define ABS_OR_FAIL(abs, path)                                                 \
  char abs[SYS_PATH_MAX];                                                      \
  if (!sys_abs_path((path), abs, sizeof(abs)))                                 \
    return (path) ? -ENAMETOOLONG : -EFAULT;

/**
 * @brief access: does @p path exist?
 *
 * There is no permission model (everything is root's and allowed), so
 * existence is the whole answer for every mode. This is what makes vi
 * stop marking files [Readonly].
 */
long sys_access(const char *path, int mode) {
  (void)mode;
  ABS_OR_FAIL(abs, path);
  return probe(abs, NULL);
}

long sys_openat(int dirfd, const char *path, int flags, int mode) {
  /* Only absolute paths or paths relative to the (fixed) cwd. */
  if (path && path[0] != '/' && dirfd != AT_FDCWD)
    return -ENOSYS;
  return sys_open(path, flags, mode);
}

long sys_ftruncate(int fd, long length) {
  if (length < 0 || length > (long)UINT32_MAX)
    return -EINVAL;
  file_t *file = fd_file(get_current_task(), fd);
  if (!file)
    return -EBADF;
  if (!file->vfs || !(file->vfs->flags & VFS_O_WRONLY))
    return -EINVAL; /* a pipe, or not open for writing */
  return vfs_truncate(file->vfs, (uint32_t)length);
}

long sys_truncate(const char *path, long length) {
  if (length < 0 || length > (long)UINT32_MAX)
    return -EINVAL;
  ABS_OR_FAIL(abs, path);
  VFS_File *f = vfs_open(abs, VFS_O_WRONLY);
  if (IS_ERR(f))
    return PTR_ERR(f);
  if (!f)
    return -ENOENT;
  long ret = vfs_truncate(f, (uint32_t)length);
  vfs_close(f);
  return ret;
}

long sys_unlink(const char *path) {
  ABS_OR_FAIL(abs, path);
  bool is_dir = false;
  long err = probe(abs, &is_dir);
  if (err < 0)
    return err;
  if (is_dir)
    return -EISDIR;
  return vfs_unlink(abs) ? 0 : -EROFS;
}

long sys_rmdir(const char *path) {
  ABS_OR_FAIL(abs, path);
  bool is_dir = false;
  long err = probe(abs, &is_dir);
  if (err < 0)
    return err;
  if (!is_dir)
    return -ENOTDIR;
  /* Exists and is a directory: failing means it still has entries (or
   * the filesystem cannot delete at all). */
  return vfs_unlink(abs) ? 0 : -ENOTEMPTY;
}

long sys_mkdir(const char *path, int mode) {
  (void)mode;
  ABS_OR_FAIL(abs, path);
  if (probe(abs, NULL) == 0)
    return -EEXIST;
  /* Missing parent, or a filesystem without mkdir. */
  return vfs_mkdir(abs) ? 0 : -ENOENT;
}

long sys_rename(const char *from, const char *to) {
  ABS_OR_FAIL(abs_from, from);
  ABS_OR_FAIL(abs_to, to);
  return vfs_rename(abs_from, abs_to);
}

/* Everything is in memory or written through: nothing to flush. */
long sys_fsync(int fd) { return fd_file(get_current_task(), fd) ? 0 : -EBADF; }
