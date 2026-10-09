/*
 * Syscalls: open and close files.
 *
 * Implements sys_open and sys_close for allocating and freeing file
 * descriptor entries in the current task's file descriptor table.
 */

#include <hubble/errno.h>
#include <hubble/syscalls.h>

#include <fs/vfs/file.h>
#include <fs/vfs/vfs.h>
#include <smp/fdtable.h>
#include <smp/scheduler.h>
#include <smp/task.h>

#include "syscall_entry.h"

/* Linux open(2) flags (x86_64). */
#define L_O_ACCMODE 0x3
#define L_O_WRONLY 0x1
#define L_O_RDWR 0x2
#define L_O_CREAT 0x40
#define L_O_EXCL 0x80
#define L_O_TRUNC 0x200
#define L_O_APPEND 0x400
#define L_O_NONBLOCK 0x800
#define L_O_DIRECTORY 0x10000
#define L_O_CLOEXEC 0x80000

/* Linux open flags -> VFS_O_* (the two number their flags differently;
 * passing Linux values through made O_CREAT read as VFS_O_TRUNC). */
static int vfs_open_flags(int flags) {
  int vfs;
  switch (flags & L_O_ACCMODE) {
  case L_O_WRONLY:
    vfs = VFS_O_WRONLY;
    break;
  case L_O_RDWR:
    vfs = VFS_O_RDWR;
    break;
  default:
    vfs = VFS_O_RDONLY;
    break;
  }
  if (flags & L_O_CREAT)
    vfs |= VFS_O_CREAT;
  if (flags & L_O_EXCL)
    vfs |= VFS_O_EXCL;
  if (flags & L_O_TRUNC)
    vfs |= VFS_O_TRUNC;
  if (flags & L_O_APPEND)
    vfs |= VFS_O_APPEND;
  return vfs;
}

/**
 * @brief Open a file.
 *
 * @param path  Path to the file (relative paths are taken from "/").
 * @param flags Linux open flags (O_RDONLY, O_CREAT, O_TRUNC, ...).
 * @param mode  Permissions for a created file (no permission model yet).
 *
 * @return File descriptor number on success, or negative errno.
 */
long sys_open(const char *path, int flags, int mode) {
  (void)mode;
  task_t *current = get_current_task();

  char abs[SYS_PATH_MAX];
  if (!sys_abs_path(path, abs, sizeof(abs)))
    return path ? -ENAMETOOLONG : -EFAULT;

  VFS_File *file = vfs_open(abs, vfs_open_flags(flags));
  if (IS_ERR(file))
    return PTR_ERR(file);
  if (!file)
    return -ENOENT;

  if ((flags & L_O_DIRECTORY) && !file->node->is_dir) {
    vfs_close(file);
    return -ENOTDIR;
  }

  file_t *f =
      file_from_vfs(file, flags & (L_O_ACCMODE | L_O_APPEND | L_O_NONBLOCK));
  if (!f) {
    vfs_close(file);
    return -ENOMEM;
  }

  int fd = fd_install(current, f, 0, (flags & L_O_CLOEXEC) ? FD_CLOEXEC : 0);
  if (fd < 0)
    file_put(f);
  return fd;
}

/**
 * @brief Close a file descriptor.
 *
 * Frees the descriptor slot; the open file itself closes once no other
 * descriptor (dup()ed, or inherited across fork()) refers to it.
 *
 * @param fd File descriptor to close.
 *
 * @return 0 on success, or -EBADF.
 */
long sys_close(int fd) { return fd_close(get_current_task(), fd); }
