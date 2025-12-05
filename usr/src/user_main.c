// #include <sys/syscall.h>

// void _start(void)
// {
// 	syscall3(SYS_write, 1, "Hello\n", 5);

// 	while (1)
// 		__asm__ volatile("hlt");
// }

// user_main.c
#include <stddef.h>
#include <stdint.h>

// Номера системных вызовов
#define SYS_write 1

// Обёртки для syscall (только 3 аргумента для write)
static inline long syscall3(long num, long arg1, long arg2, long arg3)
{
	long ret;
	__asm__ volatile(
	    "movq %1, %%rax\n"
	    "movq %2, %%rdi\n"
	    "movq %3, %%rsi\n"
	    "movq %4, %%rdx\n"
	    "syscall\n"
	    "movq %%rax, %0\n"
	    : "=r"(ret)
	    : "r"(num), "r"(arg1), "r"(arg2), "r"(arg3)
	    : "rax", "rdi", "rsi", "rdx", "rcx", "r11", "memory");
	return ret;
}

void _start(void)
{

	syscall3(SYS_write, 1, (long)"Hello\n", 5);

	while (1)
		asm volatile("hlt");
}
