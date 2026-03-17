#include <stdio.h>
#include <stddef.h>

#include "sys/syscall.h"

FILE *__stdoutp;
FILE *__stdinp;
FILE *__stderrp;

int syscall_write(struct FILE *stream, const char *buffer, int len)
{
	(void)stream;
	return syscall3(SYS_write, 1, (long)buffer, len);
}

int syscall_read(FILE *stream, char *buffer, int len)
{
	(void)stream;
	return syscall3(SYS_read, 0, (long)buffer, len);
}

void stdio_init(FILE *in, FILE *out, FILE *err)
{
	__stdinp = in;
	__stdoutp = out;
	__stderrp = err;
}

void libc_stdio_init(void)
{
	static FILE out = {.write = syscall_write};
	static FILE in = {.read = syscall_read};
	static FILE err = {.write = syscall_write};

	stdio_init(&in, &out, &err);
}

void libc_init(void)
{
	libc_stdio_init();
}
