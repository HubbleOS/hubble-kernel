// user_main.c
#include <stddef.h>
#include <stdint.h>

#include <sys/syscall.h>
#include <stdio.h>

void _start(void)
{
	libc_init();

	printf("Hello from user space!\n");

	while (1)
		;
}
