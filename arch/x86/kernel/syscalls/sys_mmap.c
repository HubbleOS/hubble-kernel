#include <stddef.h>
#include <sys/syscall.h>
#include "io.h"

long sys_mmap(void *a1, size_t a2, int a3, int a4, int a5, long a6)
{
	return 0;
}
long sys_fb(void *a1, size_t a2, int a3, int a4, int a5, long a6)
{
	outb(0x3f8, 'F');
	return 0;
}
