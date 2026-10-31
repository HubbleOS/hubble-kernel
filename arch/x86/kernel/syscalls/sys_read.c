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

#define SEEK_SET 0
#define SEEK_CUR 1

/**
 * @brief pread64: read at @p offset, leaving the file position as it was
 *
 * Seeks there and back, so - unlike Linux - not atomic against another
 * user of the same open file. Pipes and sockets fail with -ESPIPE.
 */
long sys_pread64(int fd, char *buffer, size_t len, int64_t offset) {
  if (offset < 0)
    return -EINVAL;
  long saved = sys_lseek(fd, 0, SEEK_CUR);
  if (saved < 0)
    return saved;
  long r = sys_lseek(fd, (uint64_t)offset, SEEK_SET);
  if (r < 0)
    return r;
  long n = sys_read(fd, buffer, len);
  sys_lseek(fd, (uint64_t)saved, SEEK_SET);
  return n;
}
