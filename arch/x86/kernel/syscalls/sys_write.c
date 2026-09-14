
#include "syscall_entry.h"
#include <hubble/errno.h>
#include <hubble/printk.h>
#include <hubble/syscalls.h>

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
  if (!buffer) {
    return -EINVAL;
  }
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
  if (!file) {
    return -ENOENT;
  }

  size_t written = vfs_write(file, buffer, len);
  return (long)written;
}
