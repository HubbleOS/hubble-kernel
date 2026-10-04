/*
 * Syscalls: ioctl and fcntl.
 *
 * Minimal versions: no file descriptor flag carries behaviour yet, and
 * no device implements an ioctl.
 */

#include <hubble/errno.h>
#include <hubble/syscalls.h>

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

/**
 * @brief ioctl: no device supports any request yet.
 *
 * -ENOTTY is the honest answer: isatty() then reports "not a terminal"
 * and callers such as ls fall back to plain output. Reporting a terminal
 * (TCGETS) would also make busybox switch to its own line editor, which
 * echoes input on top of the tty's echo.
 */
long sys_ioctl(int fd, unsigned long request, void *arg) {
  (void)request;
  (void)arg;
  if (!fd_is_open(fd))
    return -EBADF;
  return -ENOTTY;
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
