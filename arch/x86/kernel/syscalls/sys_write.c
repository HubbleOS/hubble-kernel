#include "syscall_entry.h"
#include <fs/vfs/file.h>
#include <hubble/errno.h>
#include <hubble/syscalls.h>
#include <smp/fdtable.h>

/**
 * @brief Write to a file descriptor.
 *
 * Writes @p len bytes from the buffer @p buffer to the file descriptor
 * @p fd. Standard output is an ordinary descriptor: usually the console
 * tty, but whatever the shell redirected it to.
 *
 * @param fd     File descriptor to write to.
 * @param buffer Source buffer containing data to write.
 * @param len    Number of bytes to write.
 *
 * @return Number of bytes written, or negative errno.
 */
long sys_write(int fd, const char *buffer, size_t len) {
  file_t *file = fd_file(get_current_task(), fd);
  if (!file)
    return -EBADF;
  if (!file->ops->write)
    return -EBADF; /* e.g. a pipe's read end */
  if (len == 0)
    return 0;
  if (!buffer)
    return -EFAULT;
  return file->ops->write(file, buffer, len);
}
