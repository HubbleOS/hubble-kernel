#include <stddef.h>
#include <stdint.h>

#include <sys/syscall.h>
#include <syscalls/syscall.h>

// syscall_fn_t syscall_table[SYSCALL_COUNT] = {
//     [SYS_write] = sys_write,
//     [SYS_read] = sys_read,
// };

// typedef struct
// {
// 	uint64_t rax; // syscall number
// 	uint64_t r9;  // arg6
// 	uint64_t r8;  // arg5
// 	uint64_t r10; // arg4
// 	uint64_t rdx; // arg3
// 	uint64_t rsi; // arg2
// 	uint64_t rdi; // arg1
// } syscall_regs_t;

// long syscall_handler(syscall_regs_t *regs)
// {
// 	long syscall_num = regs->rax;

// 	if (syscall_num < 0 || syscall_num >= SYSCALL_COUNT)
// 		return -1;

// 	syscall_fn_t fn = syscall_table[syscall_num];
// 	if (fn == NULL)
// 		return -1;

// 	return fn(regs->rdi, regs->rsi, regs->rdx,
// 		  regs->r10, regs->r8, regs->r9);
// }
