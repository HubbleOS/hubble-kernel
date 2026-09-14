#include <hubble/errno.h>
#include <hubble/string.h>
#include <hubble/syscall.h>
#include <hubble/syscalls.h>

long sys_getcwd(char *buf, size_t size) {
  if (size < 2)
    return -ERANGE;

  buf[0] = '/';
  buf[1] = '\0';

  return 2;
}
