#pragma once

#define SYS_WRITE 1
#define SYS_READ 2

typedef int (*syscall_t)(int, int, int);

int syscall_dispatch(int number, int arg0, int arg1, int arg2);
