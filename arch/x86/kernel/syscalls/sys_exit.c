#include <hubble/errno.h>
#include <hubble/string.h>
#include <hubble/syscall.h>
#include <hubble/syscalls.h>
#include <smp/scheduler.h>

long exit_stub(long a1) {
  task_exit(a1);
  __builtin_unreachable();
}
