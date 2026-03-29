#include <stddef.h>
#include <stdint.h>

#include <hubble/syscall.h>
#include <syscalls/syscall_entry.h>
#include <hubble/syscalls.h>

uint64_t syscall_handler_wrapper(registers_t *regs)
{
	regs->rax = syscall_handler(
	    regs->rax,
	    regs->rdi,
	    regs->rsi,
	    regs->rdx,
	    regs->r10,
	    regs->r8,
	    regs->r9);

	return regs->rax;
}
