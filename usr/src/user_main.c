// user_main.c
#include <stddef.h>
#include <stdint.h>

#include <syscall.h>
#include "libc.h"
#include <stdio.h>

void _start(void)
{
	libc_init();

	putchar('X');
	putchar('\n');

	printf("Hello from user space!\n");

	while (1)
		;
}
