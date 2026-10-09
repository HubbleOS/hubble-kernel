/*
 * Syscalls that make descriptors share an open file: dup, dup2, dup3 -
 * and pipe/pipe2, which create one.
 */

#include <hubble/errno.h>
#include <hubble/syscalls.h>

#include <fs/vfs/file.h>
#include <smp/fdtable.h>

#include "syscall_entry.h"

#define L_O_NONBLOCK 0x800
#define L_O_CLOEXEC 0x80000

long sys_dup(int oldfd) {
  task_t *task = get_current_task();
  file_t *file = fd_file(task, oldfd);
  if (!file)
    return -EBADF;

  int fd = fd_install(task, file_get(file), 0, 0);
  if (fd < 0)
    file_put(file);
  return fd;
}

static long dup_to(int oldfd, int newfd, int fd_flags) {
  task_t *task = get_current_task();
  file_t *file = fd_file(task, oldfd);
  if (!file)
    return -EBADF;
  if (newfd < 0 || newfd >= MAX_FDS)
    return -EBADF;
  fd_install_at(task, newfd, file, fd_flags);
  return newfd;
}

long sys_dup2(int oldfd, int newfd) {
  if (oldfd == newfd) /* nothing to do, but oldfd must be open */
    return fd_file(get_current_task(), oldfd) ? newfd : -EBADF;
  return dup_to(oldfd, newfd, 0);
}

long sys_dup3(int oldfd, int newfd, int flags) {
  if (oldfd == newfd || (flags & ~L_O_CLOEXEC))
    return -EINVAL;
  return dup_to(oldfd, newfd, (flags & L_O_CLOEXEC) ? FD_CLOEXEC : 0);
}

long sys_pipe2(int fds[2], int flags) {
  if (!fds)
    return -EFAULT;
  if (flags & ~(L_O_CLOEXEC | L_O_NONBLOCK))
    return -EINVAL;

  file_t *read_end, *write_end;
  int err = pipe_create(&read_end, &write_end);
  if (err < 0)
    return err;
  if (flags & L_O_NONBLOCK) {
    read_end->flags |= FILE_O_NONBLOCK;
    write_end->flags |= FILE_O_NONBLOCK;
  }

  task_t *task = get_current_task();
  int fd_flags = (flags & L_O_CLOEXEC) ? FD_CLOEXEC : 0;
  int rfd = fd_install(task, read_end, 0, fd_flags);
  if (rfd < 0) {
    file_put(read_end);
    file_put(write_end);
    return rfd;
  }
  int wfd = fd_install(task, write_end, 0, fd_flags);
  if (wfd < 0) {
    fd_close(task, rfd);
    file_put(write_end);
    return wfd;
  }

  fds[0] = rfd;
  fds[1] = wfd;
  return 0;
}

long sys_pipe(int fds[2]) { return sys_pipe2(fds, 0); }
