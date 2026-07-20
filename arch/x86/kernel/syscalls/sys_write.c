/*
 * Syscall: write to a file descriptor.
 *
 * Writes data from a user buffer to the kernel file associated with
 * the given file descriptor.  Writes to fd 1 (stdout) are redirected
 * to the kernel console via printk.
 */

#include <hubble/printk.h>
#include <hubble/syscalls.h>

#include "syscall_entry.h"

/**
 * @brief Write to a file descriptor.
 *
 * Writes @p len bytes from the buffer @p buffer to the file descriptor
 * @p fd.  If @p fd is 1 (stdout), output is sent directly to the kernel
 * console instead of through the VFS layer.
 *
 * @param fd     File descriptor to write to.
 * @param buffer Source buffer containing data to write.
 * @param len    Number of bytes to write.
 *
 * @return Number of bytes written on success, or -1 on error.
 */
long sys_write(int fd, const char *buffer, size_t len) {
  if (!buffer || len == 0)
    return -1;
  if (fd == 1) {
    printk("%.*s", (int)len, buffer);
    return len;
  }
  if (fd == 2) {
    printk(KERN_ERR "%.*s", (int)len, buffer);
    return len;
  }
  task_t *task = get_current_task();

  fd_entry_t *fd_entry = task_get_fd(task, fd);

  VFS_File *file = fd_entry->data;
  if (!file)
    return -1;

  size_t written = vfs_write(file, buffer, len);
  return (long)written;
}
