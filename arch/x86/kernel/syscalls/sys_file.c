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
  if (!path || size < 2)
    return NULL;
  task_t *task = get_current_task();
  const char *cwd = task && task->fdtable.cwd[0] ? task->fdtable.cwd : "/";

  /* buf is built as "/a/b" components; len 0 stands for "/". The cwd is
   * walked first for a relative path, then the path itself. */
  size_t len = 0;
  for (int pass = path[0] == '/'; pass < 2; pass++) {
    const char *p = pass == 0 ? cwd : path;
    while (*p) {
      while (*p == '/')
        p++;
      const char *name = p;
      while (*p && *p != '/')
        p++;
      size_t n = (size_t)(p - name);

      if (n == 0 || (n == 1 && name[0] == '.'))
        continue;
      if (n == 2 && name[0] == '.' && name[1] == '.') {
        while (len > 0 && buf[--len] != '/')
          ;
        continue;
      }
      if (len + 1 + n + 1 > size)
        return NULL;
      buf[len++] = '/';
      memcpy(buf + len, name, n);
      len += n;
    }
  }
  if (len == 0)
    buf[len++] = '/';
  buf[len] = '\0';
  return buf;
}

/* Open @p path just to learn whether it exists and what it is. */
long probe(const char *abs, bool *is_dir) {
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
  /* Only absolute paths or paths relative to the cwd. */
  if (path && path[0] != '/' && dirfd != AT_FDCWD)
    return -ENOSYS;
  return sys_open(path, flags, mode);
}

long sys_ftruncate(int fd, long length) {
  if (length < 0)
    return -EINVAL;
  file_t *file = fd_file(get_current_task(), fd);
  if (!file)
    return -EBADF;
  if (!file->ops->truncate)
    return -EINVAL; /* e.g. a pipe */
  return file->ops->truncate(file, (uint64_t)length);
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
  ABS_OR_FAIL(abs, path);
  if (probe(abs, NULL) == 0)
    return -EEXIST;
  /* Missing parent, or a filesystem without mkdir. */
  if (!vfs_mkdir(abs))
    return -ENOENT;
  /* Filesystems without modes keep their default. */
  vfs_chmod(abs, (uint32_t)mode & ~get_current_task()->umask);
  return 0;
}

long sys_chmod(const char *path, unsigned int mode) {
  ABS_OR_FAIL(abs, path);
  return vfs_chmod(abs, mode);
}

long sys_fchmod(int fd, unsigned int mode) {
  file_t *file = fd_file(get_current_task(), fd);
  if (!file)
    return -EBADF;
  if (!file->vfs || !file->vfs->path)
    return -EINVAL; /* a pipe, socket or pty: no mode to set */
  return vfs_chmod(file->vfs->path, mode);
}

long sys_umask(unsigned int mask) {
  task_t *task = get_current_task();
  uint32_t old = task->umask;
  task->umask = mask & 0777;
  return old;
}

long sys_readlink(const char *path, char *buf, size_t size) {
  if (!buf || !size)
    return -EINVAL;
  ABS_OR_FAIL(abs, path);
  return vfs_readlink(abs, buf, size);
}

/* Advisory whole-file locks. Accepted but not enforced: no lock is ever
 * held against anyone. Enough for a single user (tmux locks its socket
 * while the server starts), not for programs that race on a lock. */
long sys_flock(int fd, int op) {
  (void)op;
  return fd_file(get_current_task(), fd) ? 0 : -EBADF;
}

long sys_rename(const char *from, const char *to) {
  ABS_OR_FAIL(abs_from, from);
  ABS_OR_FAIL(abs_to, to);
  return vfs_rename(abs_from, abs_to);
}

/* Everything is in memory or written through: nothing to flush. */
long sys_fsync(int fd) { return fd_file(get_current_task(), fd) ? 0 : -EBADF; }
