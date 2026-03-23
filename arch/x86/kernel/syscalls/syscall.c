#include <stddef.h>
#include <stdint.h>

#include <sys/syscall.h>
#include <syscalls/syscall_entry.h>

typedef long (*syscall_fn_t)(long arg1, long arg2, long arg3, long arg4, long arg5, long arg6);

#define SYSCALL_COUNT 256

syscall_fn_t syscall_table[SYSCALL_COUNT] = {
    [SYS_write] = (syscall_fn_t)sys_write,
    [0] = (syscall_fn_t)sys_read,
    [3] = (syscall_fn_t)sys_mmap,
    [4] = (syscall_fn_t)sys_open,
    [5] = (syscall_fn_t)sys_close,
    [6] = (syscall_fn_t)sys_spawn};

uint64_t syscall_handler(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3,
			 uint64_t a4, uint64_t a5, uint64_t a6)
{
	if (num >= SYSCALL_COUNT || !syscall_table[num])
		return -1;

	return syscall_table[num](a1, a2, a3, a4, a5, a6);
}

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
