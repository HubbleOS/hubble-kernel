#pragma once

#include <stddef.h>

#define SYS_write 1
#define SYS_read 2
#define SYSCALL_COUNT 3

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

#define do_syscall do_syscall_fast

#define syscall0(n) do_syscall(n, 0, 0, 0, 0, 0, 0)
#define syscall1(n, a1) do_syscall(n, a1, 0, 0, 0, 0, 0)
#define syscall2(n, a1, a2) do_syscall(n, a1, a2, 0, 0, 0, 0)
#define syscall3(n, a1, a2, a3) do_syscall(n, a1, a2, a3, 0, 0, 0)
#define syscall4(n, a1, a2, a3, a4) do_syscall(n, a1, a2, a3, a4, 0, 0)
#define syscall5(n, a1, a2, a3, a4, a5) do_syscall(n, a1, a2, a3, a4, a5, 0)
#define syscall6(n, a1, a2, a3, a4, a5, a6) do_syscall(n, a1, a2, a3, a4, a5, a6)

typedef long (*syscall_fn_t)(long arg1, long arg2, long arg3, long arg4, long arg5, long arg6);
