#include <stddef.h>
#include <stdint.h>

#include <hubble/syscall.h>
#include <hubble/syscalls.h>
#include <syscalls/syscall_entry.h>

#include <smp/scheduler.h>
#include <smp/task.h>

uint64_t syscall_handler_wrapper(registers_t *regs) {
  task_t *task = get_current_task();

  task->exec.in_syscall_rsp = 0;
  task->exec.in_syscall = true;

  /* fork() needs the raw trap frame (return RIP/RFLAGS, all GPRs) to set up
   * the child's resume state, which the generic (num, a1..a6) dispatch
   * below has no room to carry - so it's special-cased here instead of
   * going through syscall_table. execve() is the same story in reverse:
   * on success it doesn't return a value at all, it rewrites where the
   * sysret in syscall_entry.asm lands (RCX / %gs:8), which only the raw
   * trap frame gives it access to. */
  if (regs->rax == SYS_fork) {
    regs->rax = sys_fork(regs);
  } else if (regs->rax == SYS_execve) {
    regs->rax = sys_execve(regs);
  } else {
    regs->rax = syscall_handler(regs->rax, regs->rdi, regs->rsi, regs->rdx,
                                regs->r10, regs->r8, regs->r9);
  }

  task->exec.in_syscall = false;
  return regs->rax;
}
