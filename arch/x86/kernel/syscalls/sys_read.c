/*
 * Syscall: read from a file descriptor.
 *
 * Reads data from the kernel file associated with the given file
 * descriptor into a user-provided buffer.
 */

#include <hubble/syscalls.h>

#include "syscall_entry.h"

/**
 * @brief Read from a file descriptor.
 *
 * Reads up to @p len bytes from the file descriptor @p fd into the
 * buffer pointed to by @p buffer.
 *
 * @param fd      File descriptor to read from.
 * @param buffer  Destination buffer for the read data.
 * @param len     Maximum number of bytes to read.
 *
 * @return Number of bytes read on success, or -1 on error.
 */
long sys_read(int fd, char *buffer, size_t len) {
  if (fd < 0 || fd >= MAX_FDS || !buffer || len == 0) {
    return -1;
  }
  task_t *current = get_current_task();
  fd_entry_t *fd_entry = task_get_fd(current, fd);

  VFS_File *file = fd_entry->data;
  if (!file)
    return -1;

  size_t read_count = vfs_read(file, buffer, len);
  return (long)read_count;
}
