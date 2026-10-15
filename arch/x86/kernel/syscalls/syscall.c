#include <stddef.h>
#include <stdint.h>

#include <hubble/printk.h>
#include <hubble/syscall.h>
#include <hubble/syscalls.h>
#include <syscalls/abi.h>
#include <syscalls/syscall_entry.h>

#include <smp/scheduler.h>
#include <smp/task.h>

/* Linux x86_64: number in RAX, arguments in RDI RSI RDX R10 R8 R9,
 * -errno back in RAX. fork and execve need the whole trap frame. */
static uint64_t linux_dispatch(registers_t *regs) {
  if (regs->rax == SYS_fork)
    return sys_fork(regs);
  if (regs->rax == SYS_execve)
    return sys_execve(regs);
  return syscall_handler(regs->rax, regs->rdi, regs->rsi, regs->rdx,
                         regs->r10, regs->r8, regs->r9);
}

const syscall_abi_t abi_linux = {"linux", linux_dispatch};

#define ELFOSABI_SYSV 0 /* what gcc and musl emit for Linux programs */
#define ELFOSABI_LINUX 3

const syscall_abi_t *abi_for_elf(uint8_t osabi) {
  switch (osabi) {
  case ELFOSABI_SYSV:
  case ELFOSABI_LINUX:
    return &abi_linux;
  default:
    return NULL; /* e.g. 9 = FreeBSD: would need its own table */
  }
}

uint64_t syscall_handler_wrapper(registers_t *regs) {
  task_t *task = get_current_task();

  task->exec.in_syscall_rsp = 0;
  task->exec.in_syscall = true;

  if (task->abi) {
    regs->rax = task->abi->dispatch(regs);
  } else {
    /* Only exec'd user tasks get an ABI; nothing else should get here. */
    printk(KERN_ERR "[SYSCALL] pid %u has no syscall ABI\n", task->id.pid);
    regs->rax = -ENOSYS;
  }

  task->exec.in_syscall = false;
  return regs->rax;
}
