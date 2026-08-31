/*
 * Syscalls: open and close files.
 *
 * Implements sys_open and sys_close for allocating and freeing file
 * descriptor entries in the current task's file descriptor table.
 */

#include <hubble/syscalls.h>

#include <fs/vfs/vfs.h>
#include <smp/scheduler.h>
#include <smp/task.h>

#include "syscall_entry.h"

/**
 * @brief Open a file.
 *
 * Opens the file at @p path with the given @p flags and allocates a
 * new file descriptor in the current task's FD table.
 *
 * @param path  Path to the file to open.
 * @param flags Open flags (e.g. O_RDONLY, O_WRONLY).
 *
 * @return File descriptor number on success, or -1 on error.
 */
long sys_open(const char *path, int flags) {
  task_t *current = get_current_task();

  VFS_File *file = vfs_open(path, flags);
  if (IS_ERR(file) || !file)
    return -ENOENT;

  for (int i = 2; i < MAX_FDS; i++) {
    if (!current->fdtable.fds[i].data) {
      current->fdtable.fds[i].data = file;
      current->fdtable.fds[i].flags = flags;
      current->fdtable.fds[i].type = FD_FILE;
      return i;
    }
  }
  vfs_close(file);
  return -EBADF;
}

/**
 * @brief Close a file descriptor.
 *
 * Closes the file associated with the given file descriptor @p fd and
 * frees the descriptor slot in the current task's FD table.
 *
 * @param fd File descriptor to close.
 *
 * @return 0 on success, or -1 on error.
 */
long sys_close(int fd) {
  task_t *current = get_current_task();
  if (fd < 0 || fd >= MAX_FDS || !current->fdtable.fds[fd].data)
    return -EBADF;
  vfs_close(current->fdtable.fds[fd].data);
  current->fdtable.fds[fd].data = NULL;
  return 0;
}
