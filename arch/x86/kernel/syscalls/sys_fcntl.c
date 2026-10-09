/*
 * Syscalls: ioctl and fcntl, plus the fd -> character device lookup that
 * ioctl and poll share.
 */

#include <hubble/errno.h>
#include <hubble/syscalls.h>

#include <fs/vfs/dev.h>
#include <fs/vfs/file.h>
#include <fs/vfs/vfs.h>
#include <smp/fdtable.h>
#include <smp/scheduler.h>
#include <smp/task.h>

#include "syscall_entry.h"

#define F_DUPFD 0
#define F_GETFD 1
#define F_SETFD 2
#define F_GETFL 3
#define F_SETFL 4
#define F_DUPFD_CLOEXEC 1030

/* What F_SETFL may change; the access mode is fixed at open. */
#define SETFL_MASK (FILE_O_APPEND | FILE_O_NONBLOCK)

VFS_device_reg *fd_get_device(int fd) {
  file_t *file = fd_file(get_current_task(), fd);
  return file ? file_device(file) : NULL;
}

/**
 * @brief ioctl: forwarded to the device behind the fd.
 *
 * Anything that is not a device with an ioctl hook answers -ENOTTY,
 * which is also how isatty() tells a terminal from a file.
 */
long sys_ioctl(int fd, unsigned long request, void *arg) {
  if (!fd_file(get_current_task(), fd))
    return -EBADF;

  VFS_device_reg *dev = fd_get_device(fd);
  if (!dev || !dev->ioctl)
    return -ENOTTY;
  return dev->ioctl(request, arg);
}

/**
 * @brief fcntl: duplicate a descriptor, or get/set its flags.
 *
 * F_DUPFD is how a shell saves stdout around a redirection
 * (`echo x > file`), so it has to work for 0-2 like any descriptor.
 */
long sys_fcntl(int fd, int cmd, long arg) {
  task_t *task = get_current_task();
  file_t *file = fd_file(task, fd);
  if (!file)
    return -EBADF;
  fd_entry_t *entry = &task->fdtable.fds[fd];

  switch (cmd) {
  case F_DUPFD:
  case F_DUPFD_CLOEXEC: {
    if (arg < 0 || arg >= MAX_FDS)
      return -EINVAL;
    int newfd = fd_install(task, file_get(file), (int)arg,
                           cmd == F_DUPFD_CLOEXEC ? FD_CLOEXEC : 0);
    if (newfd < 0)
      file_put(file);
    return newfd;
  }
  case F_GETFD:
    return entry->fd_flags;
  case F_SETFD:
    entry->fd_flags = (int)arg & FD_CLOEXEC;
    return 0;
  case F_GETFL:
    return file->flags;
  case F_SETFL:
    file->flags = (file->flags & ~SETFL_MASK) | ((int)arg & SETFL_MASK);
    /* The VFS appends on its own flag, not ours. */
    if (file->vfs) {
      if (file->flags & FILE_O_APPEND)
        file->vfs->flags |= VFS_O_APPEND;
      else
        file->vfs->flags &= ~VFS_O_APPEND;
    }
    return 0;
  default:
    return -EINVAL;
  }
}
