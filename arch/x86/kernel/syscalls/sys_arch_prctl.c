#include <hubble/errno.h>
#include <hubble/syscalls.h>
#include <msr.h>
#include <smp/scheduler.h>
#include <smp/task.h>

#define ARCH_SET_GS 0x1001
#define ARCH_SET_FS 0x1002
#define ARCH_GET_FS 0x1003
#define ARCH_GET_GS 0x1004

/* Lower half: anything else is kernel space or non-canonical, and
 * writing a non-canonical base faults the wrmsr in the kernel. */
#define USER_ADDR_END 0x0000800000000000ULL

long arch_prctl(long a1, long a2, long a3, long a4, long a5, long a6) {
  task_t *current_task = get_current_task();

  switch (a1) {
  case ARCH_SET_FS:
    if ((uint64_t)a2 >= USER_ADDR_END)
      return -EPERM;
    current_task->mm.fs_base = a2;
    wrmsr(MSR_FS_BASE, (uint64_t)a2);
    return 0;
  case ARCH_GET_FS:
    return rdmsr(MSR_FS_BASE);
  case ARCH_SET_GS:
    if ((uint64_t)a2 >= USER_ADDR_END)
      return -EPERM;
    current_task->mm.gs_base = a2;
    /* Inside a syscall the user value is the swapped-out one; sysret's
     * swapgs puts it in GS_BASE. */
    wrmsr(MSR_KERNEL_GS_BASE, (uint64_t)a2);
    return 0;
  case ARCH_GET_GS:
    return current_task->mm.gs_base;
  default:
    return -EINVAL;
  }
}
