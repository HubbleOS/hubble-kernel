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

#define SEEK_SET 0
#define SEEK_CUR 1

/** pwrite64: write at @p offset, leaving the file position as it was
 * (not atomic, as sys_pread64). */
long sys_pwrite64(int fd, const char *buffer, size_t len, int64_t offset) {
  if (offset < 0)
    return -EINVAL;
  long saved = sys_lseek(fd, 0, SEEK_CUR);
  if (saved < 0)
    return saved;
  long r = sys_lseek(fd, (uint64_t)offset, SEEK_SET);
  if (r < 0)
    return r;
  long n = sys_write(fd, buffer, len);
  sys_lseek(fd, (uint64_t)saved, SEEK_SET);
  return n;
}
