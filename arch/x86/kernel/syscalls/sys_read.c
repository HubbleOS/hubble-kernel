#include <hubble/syscalls.h>

#include <fs/vfs/file.h>
#include <smp/fdtable.h>

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
 * @return Number of bytes read (0 at end of file), or negative errno.
 */
long sys_read(int fd, char *buffer, size_t len) {
  file_t *file = fd_file(get_current_task(), fd);
  if (!file)
    return -EBADF;
  if (!file->ops->read)
    return -EBADF; /* e.g. a pipe's write end */
  if (len == 0)
    return 0;
  if (!buffer)
    return -EFAULT;
  return file->ops->read(file, buffer, len);
}
