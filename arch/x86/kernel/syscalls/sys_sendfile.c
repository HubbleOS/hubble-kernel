#include <hubble/errno.h>
#include <hubble/syscalls.h>
#include <mm/kmalloc.h>

#include "syscall_entry.h"

#define SENDFILE_CHUNK 4096
#define SEEK_SET 0
#define SEEK_CUR 1

/**
 * @brief Copy up to @p count bytes from @p in_fd to @p out_fd
 *
 * A read/write loop through a kernel buffer. With @p offset, reads start
 * there, *offset is advanced and in_fd's own position is left as it was.
 *
 * @return Bytes copied, or negative errno if nothing was
 */
long sys_sendfile(int out_fd, int in_fd, int64_t *offset, size_t count) {
  long saved = 0;
  if (offset) {
    saved = sys_lseek(in_fd, 0, SEEK_CUR);
    if (saved < 0)
      return saved;
    long r = sys_lseek(in_fd, (uint64_t)*offset, SEEK_SET);
    if (r < 0)
      return r;
  }

  char *buf = kmalloc(SENDFILE_CHUNK, GFP_KERNEL);
  if (!buf)
    return -ENOMEM;

  long total = 0, err = 0;
  while ((size_t)total < count) {
    size_t want = count - total < SENDFILE_CHUNK ? count - total
                                                 : SENDFILE_CHUNK;
    long n = sys_read(in_fd, buf, want);
    if (n <= 0) {
      err = n;
      break;
    }
    long done = 0;
    while (done < n) {
      long w = sys_write(out_fd, buf + done, n - done);
      if (w <= 0) {
        err = w ? w : -EIO;
        break;
      }
      done += w;
    }
    total += done;
    if (err || (size_t)n < want)
      break;
  }
  kfree(buf);

  if (offset) {
    *offset += total;
    sys_lseek(in_fd, (uint64_t)saved, SEEK_SET);
  }
  return total ? total : err;
}
