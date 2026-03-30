#include <hubble/syscall.h>
#include <hubble/syscalls.h>

typedef long (*syscall_fn_t)(long arg1, long arg2, long arg3, long arg4, long arg5, long arg6);

syscall_fn_t syscall_table[SYSCALL_COUNT] = {
    [SYS_write] = (syscall_fn_t)sys_write,
    [SYS_read] = (syscall_fn_t)sys_read,
    [SYS_mmap] = (syscall_fn_t)sys_mmap,
    [SYS_open] = (syscall_fn_t)sys_open,
    [SYS_close] = (syscall_fn_t)sys_close,
    [SYS_spawn] = (syscall_fn_t)sys_spawn,
    [SYS_spawn_file] = (syscall_fn_t)sys_spawn_file,
};

uint64_t syscall_handler(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3,
			 uint64_t a4, uint64_t a5, uint64_t a6)
{
	if (num >= SYSCALL_COUNT || !syscall_table[num])
		return -1;

	return syscall_table[num](a1, a2, a3, a4, a5, a6);
}
