/*
 * Syscalls: ioctl and fcntl, plus the fd -> character device lookup that
 * ioctl and poll share.
 */

#include <hubble/errno.h>
#include <hubble/syscalls.h>

#include <fs/vfs/dev.h>
#include <fs/vfs/vfs.h>
#include <smp/scheduler.h>
#include <smp/task.h>

#include "syscall_entry.h"

#define F_GETFD 1
#define F_SETFD 2
#define F_GETFL 3
#define F_SETFL 4

/* fds 0-2 are served by sys_read/sys_write without a table entry. */
static bool fd_is_open(int fd) {
  if (fd >= 0 && fd <= 2)
    return true;
  fd_entry_t *entry = task_get_fd(get_current_task(), fd);
  return entry && entry->data;
}

VFS_device_reg *fd_get_device(int fd) {
  /* stdin/stdout/stderr are the console tty. */
  if (fd >= 0 && fd <= 2)
    return dev_vfs_find_device(NULL, "tty0");

  fd_entry_t *entry = task_get_fd(get_current_task(), fd);
  if (!entry || !entry->data || entry->type != FD_FILE)
    return NULL;

  VFS_File *file = entry->data;
  if (!file->node || !file->node->fs || file->node->fs->type != FS_DEV)
    return NULL;
  return (VFS_device_reg *)file->node->fs_node;
}

/**
 * @brief ioctl: forwarded to the device behind the fd.
 *
 * Anything that is not a device with an ioctl hook answers -ENOTTY,
 * which is also how isatty() tells a terminal from a file.
 */
long sys_ioctl(int fd, unsigned long request, void *arg) {
  if (!fd_is_open(fd))
    return -EBADF;

  VFS_device_reg *dev = fd_get_device(fd);
  if (!dev || !dev->ioctl)
    return -ENOTTY;
  return dev->ioctl(request, arg);
}

/**
 * @brief fcntl: accept the flag queries libc makes on every open.
 *
 * FD_CLOEXEC and the status flags are accepted but not enforced yet.
 */
long sys_fcntl(int fd, int cmd, long arg) {
  (void)arg;
  if (!fd_is_open(fd))
    return -EBADF;

  switch (cmd) {
  case F_GETFD:
  case F_SETFD:
  case F_SETFL:
    return 0;
  case F_GETFL: {
    fd_entry_t *entry = task_get_fd(get_current_task(), fd);
    return (entry && entry->data) ? entry->flags : 0;
  }
  default:
    return -EINVAL;
  }
}
