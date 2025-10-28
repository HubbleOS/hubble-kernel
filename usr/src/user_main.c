#include <stdio.h>

void _start(void)
{
	putchar('H');
	putchar('e');
	putchar('l');
	putchar('l');
	putchar('o');
	putchar(' ');
	putchar('W');
	putchar('o');
	putchar('r');
	putchar('l');
	putchar('d');
	putchar('\n');

	while (1)
	{
		asm volatile("hlt");
	}
}
