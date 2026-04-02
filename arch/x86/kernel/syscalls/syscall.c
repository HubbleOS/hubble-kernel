#include <stddef.h>
#include <stdint.h>

#include <hubble/syscall.h>
#include <syscalls/syscall_entry.h>
#include <hubble/syscalls.h>

#include <smp/task.h>
#include <smp/scheduler.h>

uint64_t syscall_handler_wrapper(registers_t *regs)
{
	task_t *task = get_current_task();

	task->in_syscall = true;
	regs->rax = syscall_handler(
	    regs->rax,
	    regs->rdi,
	    regs->rsi,
	    regs->rdx,
	    regs->r10,
	    regs->r8,
	    regs->r9);
	task->in_syscall = false;
	return regs->rax;
}
