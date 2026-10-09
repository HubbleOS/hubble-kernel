/**
 * @file fdtable.c
 * @brief A task's file descriptors (see fdtable.h)
 */
#include "fdtable.h"
#include <hubble/errno.h>

#define CONSOLE_PATH "/dev/tty0"

file_t *fd_file(task_t *task, int fd) {
  if (!task || fd < 0 || fd >= MAX_FDS)
    return NULL;
  return task->fdtable.fds[fd].file;
}

int fd_install(task_t *task, file_t *file, int min_fd, int fd_flags) {
  for (int fd = min_fd < 0 ? 0 : min_fd; fd < MAX_FDS; fd++) {
    fd_entry_t *entry = &task->fdtable.fds[fd];
    if (entry->file)
      continue;
    entry->file = file;
    entry->fd_flags = fd_flags;
    return fd;
  }
  return -EMFILE;
}

void fd_install_at(task_t *task, int fd, file_t *file, int fd_flags) {
  fd_entry_t *entry = &task->fdtable.fds[fd];
  file_t *old = entry->file;
  entry->file = file_get(file);
  entry->fd_flags = fd_flags;
  /* After the swap: @p file may be the one @p fd already held. */
  if (old)
    file_put(old);
}

int fd_close(task_t *task, int fd) {
  file_t *file = fd_file(task, fd);
  if (!file)
    return -EBADF;
  task->fdtable.fds[fd].file = NULL;
  task->fdtable.fds[fd].fd_flags = 0;
  file_put(file);
  return 0;
}

void fd_copy_table(task_t *child, task_t *parent) {
  for (int fd = 0; fd < MAX_FDS; fd++) {
    fd_entry_t *from = &parent->fdtable.fds[fd];
    child->fdtable.fds[fd].file = from->file ? file_get(from->file) : NULL;
    child->fdtable.fds[fd].fd_flags = from->fd_flags;
  }
}

void fd_close_on_exec(task_t *task) {
  for (int fd = 0; fd < MAX_FDS; fd++)
    if (task->fdtable.fds[fd].file &&
        (task->fdtable.fds[fd].fd_flags & FD_CLOEXEC))
      fd_close(task, fd);
}

void fd_close_all(task_t *task) {
  for (int fd = 0; fd < MAX_FDS; fd++)
    if (task->fdtable.fds[fd].file)
      fd_close(task, fd);
}

/* One open file, shared by 0, 1 and 2 like a login's terminal. */
int fd_open_console(task_t *task) {
  VFS_File *vfs = vfs_open(CONSOLE_PATH, VFS_O_RDWR);
  if (IS_ERR(vfs))
    return PTR_ERR(vfs);
  if (!vfs)
    return -ENOENT;

  file_t *console = file_from_vfs(vfs, FILE_O_RDWR);
  if (!console) {
    vfs_close(vfs);
    return -ENOMEM;
  }

  for (int fd = 0; fd <= 2; fd++)
    fd_install_at(task, fd, console, 0);
  file_put(console); /* the three descriptors hold it now */
  return 0;
}
