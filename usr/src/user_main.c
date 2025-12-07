// user_main.c
#include <stddef.h>
#include <stdint.h>

static inline long do_syscall_int(long num, long arg1, long arg2, long arg3,
				  long arg4, long arg5, long arg6)
{
	long ret;
	__asm__ volatile(
	    "movq %1, %%rax\n"
	    "movq %2, %%rdi\n"
	    "movq %3, %%rsi\n"
	    "movq %4, %%rdx\n"
	    "movq %5, %%r10\n"
	    "movq %6, %%r8\n"
	    "movq %7, %%r9\n"
	    "int $0x80\n"
	    "movq %%rax, %0\n"
	    : "=r"(ret)
	    : "r"(num), "r"(arg1), "r"(arg2),
	      "r"(arg3), "r"(arg4), "r"(arg5), "r"(arg6)
	    : "rax", "rdi", "rsi", "rdx", "r10", "r8", "r9",
	      "rcx", "r11", "memory");
	return ret;
}

static inline long do_syscall_fast(long num, long arg1, long arg2, long arg3,
				   long arg4, long arg5, long arg6)
{
	long ret;
	__asm__ volatile(
	    "movq %1, %%rax\n"
	    "movq %2, %%rdi\n"
	    "movq %3, %%rsi\n"
	    "movq %4, %%rdx\n"
	    "movq %5, %%r10\n"
	    "movq %6, %%r8\n"
	    "movq %7, %%r9\n"
	    "syscall\n"
	    "movq %%rax, %0\n"
	    : "=r"(ret)
	    : "r"(num), "r"(arg1), "r"(arg2),
	      "r"(arg3), "r"(arg4), "r"(arg5), "r"(arg6)
	    : "rax", "rdi", "rsi", "rdx", "r10", "r8", "r9",
	      "rcx", "r11", "memory");
	return ret;
}

#define SYS_write 1

void _start(void)
{
	do_syscall_int(SYS_write, 1, (long)"Hello world from userland int\n", 30, 0, 0, 0);

	do_syscall_fast(SYS_write, 1, (long)"Hello world from userland fast\n", 31, 0, 0, 0);

	while (1)
		;
}
