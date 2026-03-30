#include <stdint.h>

void main(void)
{
	asm volatile("mov x0, #0");

	while (1)
	{
		// Increment x0
		asm volatile(
		    "add x0, x0, #1\n"
		    :
		    :
		    : "x0");
	}
}
