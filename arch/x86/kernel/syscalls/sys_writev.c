#include "syscall_entry.h"
#include <hubble/syscalls.h>
#include <stddef.h>

long sys_writev(int fd, struct iovec *iov, int iovcnt) {
  long total = 0;
  for (int i = 0; i < iovcnt; i++) {
    long n = sys_write(fd, iov[i].iov_base, iov[i].iov_len);
    if (n < 0)
      return n;
    total += n;
    if ((size_t)n < iov[i].iov_len)
      break;
  }
  return total;
}
