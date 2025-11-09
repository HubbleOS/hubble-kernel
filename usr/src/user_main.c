#include <sys/syscall.h>

void _start(void)
{
	// syscall3(SYS_write, 1, "Hello\n", 5);

	while (1)
		__asm__ volatile("hlt");
}
