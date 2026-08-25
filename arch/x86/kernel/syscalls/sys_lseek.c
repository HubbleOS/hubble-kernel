/*
 * Syscall: reposition file offset.
 *
 * Implements the lseek system call for changing the current read/write
 * position within an open file.
 */

#include <hubble/syscalls.h>

#include "syscall_entry.h"

/**
 * @brief Reposition the file offset.
 *
 * Changes the current file offset for the file descriptor @p fd
 * according to @p whence (e.g. SEEK_SET, SEEK_CUR, SEEK_END).
 *
 * @param fd     File descriptor to manipulate.
 * @param offset Offset value interpreted per @p whence.
 * @param whence Reference point for the offset.
 *
 * @return The resulting file offset on success, or -1 on error.
 */
long sys_lseek(int fd, uint64_t offset, int whence) {
  task_t *task = get_current_task();

  fd_entry_t *fd_entry = task_get_fd(task, fd);
  VFS_File *file = fd_entry->data;
  if (!file)
    return -EBADF;

  return (long)vfs_lseek(file, offset, whence);
}
