#pragma once

#include <_cheader.h>

#include <stddef.h>

_Begin_C_Header;

void syscall_init(void);

long sys_read(int, char *, size_t);
long sys_write(int, const char *, size_t);

_End_C_Header;
