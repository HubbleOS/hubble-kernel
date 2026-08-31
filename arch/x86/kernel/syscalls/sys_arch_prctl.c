#include <hubble/errno.h>
#include <hubble/syscalls.h>
#include <msr.h>
#include <smp/scheduler.h>
#include <smp/task.h>

long arch_prctl(long a1, long a2, long a3, long a4, long a5, long a6) {
  switch (a1) {
  case 0x1002:
    task_t *current_task = get_current_task();
    current_task->mm.fs_base = a2;
    wrmsr(0xC0000100, (uint64_t)a2);
    break;
  case 0x1003:
    return rdmsr(0xC0000100);
    break;
  default:
    return -ENOSYS;
    break;
  }

  return 0;
}
