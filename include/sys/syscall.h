#pragma once

#include <stddef.h>

#define SYS_write 1
#define SYS_read 2

static inline long syscall(long num, long arg1, long arg2,
			   long arg3, long arg4, long arg5, long arg6)
{
	long ret;

	// for x86_64 с syscall instruction
	__asm__ volatile(
	    "movq %1, %%rax\n" // num syscall
	    "movq %2, %%rdi\n" // arg1
	    "movq %3, %%rsi\n" // arg2
	    "movq %4, %%rdx\n" // arg3
	    "movq %5, %%r10\n" // arg4
	    "movq %6, %%r8\n"  // arg5
	    "movq %7, %%r9\n"  // arg6
	    "syscall\n"	       // or int $0x80
	    "movq %%rax, %0\n" // result
	    : "=r"(ret)
	    : "r"(num), "r"(arg1), "r"(arg2),
	      "r"(arg3), "r"(arg4), "r"(arg5), "r"(arg6)
	    : "rax", "rdi", "rsi", "rdx", "r10", "r8", "r9", "memory");

	return ret;
}

static inline long read(int fd, void *buf, unsigned long count)
{
	return syscall(SYS_read, fd, (long)buf, count, 0, 0, 0);
}

static inline long write(int fd, const void *buf, unsigned long count)
{
	return syscall(SYS_write, fd, (long)buf, count, 0, 0, 0);
}

// #pragma once

// #include <_cheader.h>

// #include <stddef.h>

typedef long (*syscall_fn_t)(long arg1, long arg2, long arg3,
			     long arg4, long arg5, long arg6);

// #define syscall(n, a1, a2, a3, a4, a5, a6) syscall_dispatcher(n, a1, a2, a3, a4, a5, a6)
// #define syscall0(n, a1) syscall_dispatcher(n, a1, 0, 0, 0, 0, 0)
// #define syscall1(n, a1, a2) syscall_dispatcher(n, a1, a2, 0, 0, 0, 0)
// #define syscall2(n, a1, a2, a3) syscall_dispatcher(n, a1, a2, a3, 0, 0, 0)
// #define syscall3(n, a1, a2, a3, a4) syscall_dispatcher(n, a1, a2, a3, a4, 0, 0)
// #define syscall4(n, a1, a2, a3, a4, a5) syscall_dispatcher(n, a1, a2, a3, a4, a5, 0)
// #define syscall5(n, a1, a2, a3, a4, a5, a6) syscall_dispatcher(n, a1, a2, a3, a4, a5, a6)
// #define syscall6(n, a1, a2, a3, a4, a5, a6) syscall_dispatcher(n, a1, a2, a3, a4, a5, a6)

// _Begin_C_Header;

// long syscall_dispatcher(long n, long a1, long a2, long a3, long a4, long a5, long a6);

long sys_read(int, char *, size_t);
long sys_write(int, const char *, size_t);
// long sys_mmap(void *, size_t, int, int, int, long);

// _End_C_Header;
