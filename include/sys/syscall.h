#pragma once

#include <_cheader.h>

#include <stddef.h>

_Begin_C_Header;

long syscall_dispatcher(long n, long a1, long a2, long a3, long a4, long a5, long a6);

#define syscall(n, a1, a2, a3, a4, a5, a6) syscall_dispatcher(n, a1, a2, a3, a4, a5, a6)

long sys_read(int, char *, size_t);
long sys_write(int, const char *, size_t);
long sys_mmap(void *, size_t, int, int, int, long);

_End_C_Header;
