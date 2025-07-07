#pragma once

#include <_cheader.h>

_Begin_C_Header;

long syscall_dispatcher(long n, long a1, long a2, long a3, long a4, long a5, long a6);

long sys_read(int, char *, size_t);
long sys_write(int, const char *, size_t);

_End_C_Header;
