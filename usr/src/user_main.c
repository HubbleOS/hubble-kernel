// user_main.c
#include <stddef.h>
#include <stdint.h>

#include <syscall.h>
#include "libc.h"
#include <stdio.h>

#include "sys/syscall.h"

void _start(void)
{
	libc_init();

	putchar('X');
	putchar('\n');

	printf("Hello from user space!\n");

	syscall3(42, 0, 0, 0);

	while (1)
		;
}
