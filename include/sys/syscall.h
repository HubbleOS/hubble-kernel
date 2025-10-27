#pragma once

#include <_cheader.h>

#include <stddef.h>

#define syscall(n, a1, a2, a3, a4, a5, a6) syscall_dispatcher(n, a1, a2, a3, a4, a5, a6)
#define syscall0(n, a1) syscall_dispatcher(n, a1, 0, 0, 0, 0, 0)
#define syscall1(n, a1, a2) syscall_dispatcher(n, a1, a2, 0, 0, 0, 0)
#define syscall2(n, a1, a2, a3) syscall_dispatcher(n, a1, a2, a3, 0, 0, 0)
#define syscall3(n, a1, a2, a3, a4) syscall_dispatcher(n, a1, a2, a3, a4, 0, 0)
#define syscall4(n, a1, a2, a3, a4, a5) syscall_dispatcher(n, a1, a2, a3, a4, a5, 0)
#define syscall5(n, a1, a2, a3, a4, a5, a6) syscall_dispatcher(n, a1, a2, a3, a4, a5, a6)
#define syscall6(n, a1, a2, a3, a4, a5, a6) syscall_dispatcher(n, a1, a2, a3, a4, a5, a6)

_Begin_C_Header;

long syscall_dispatcher(long n, long a1, long a2, long a3, long a4, long a5, long a6);

long sys_read(int, char *, size_t);
long sys_write(int, const char *, size_t);
long sys_mmap(void *, size_t, int, int, int, long);

_End_C_Header;
