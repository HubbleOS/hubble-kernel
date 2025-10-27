#include <stddef.h>

#include <sys/syscall.h>
#include <sys/syscall_nums.h>

long syscall_dispatcher(long n, long a1, long a2, long a3, long a4, long a5, long a6)
{
	switch (n)
	{
	case SYS_write:
		return sys_write((int)a1, (const char *)a2, (size_t)a3);

	case SYS_read:
		return sys_read((int)a1, (char *)a2, (size_t)a3);

	case SYS_mmap:
		return sys_mmap((void *)a1, (size_t)a2, (int)a3, (int)a4, (int)a5, (long)a6);

	default:
		return -1; // unknown syscall
	}
}

// static __inline long __syscall0(long n)
// {
// 	unsigned long ret;
// 	__asm__ __volatile__("syscall" : "=a"(ret) : "a"(n) : "rcx", "r11", "memory");
// 	return ret;
// }

// static __inline long __syscall1(long n, long a1)
// {
// 	unsigned long ret;
// 	__asm__ __volatile__("syscall" : "=a"(ret) : "a"(n), "D"(a1) : "rcx", "r11", "memory");
// 	return ret;
// }

// static __inline long __syscall2(long n, long a1, long a2)
// {
// 	unsigned long ret;
// 	__asm__ __volatile__("syscall" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2)
// 						 : "rcx", "r11", "memory");
// 	return ret;
// }

// static __inline long __syscall3(long n, long a1, long a2, long a3)
// {
// 	unsigned long ret;
// 	__asm__ __volatile__("syscall" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2),
// 												 "d"(a3) : "rcx", "r11", "memory");
// 	return ret;
// }

// static __inline long __syscall4(long n, long a1, long a2, long a3, long a4)
// {
// 	unsigned long ret;
// 	register long r10 __asm__("r10") = a4;
// 	__asm__ __volatile__("syscall" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2),
// 												 "d"(a3), "r"(r10) : "rcx", "r11", "memory");
// 	return ret;
// }

// static __inline long __syscall5(long n, long a1, long a2, long a3, long a4, long a5)
// {
// 	unsigned long ret;
// 	register long r10 __asm__("r10") = a4;
// 	register long r8 __asm__("r8") = a5;
// 	__asm__ __volatile__("syscall" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2),
// 												 "d"(a3), "r"(r10), "r"(r8) : "rcx", "r11", "memory");
// 	return ret;
// }

// static __inline long __syscall6(long n, long a1, long a2, long a3, long a4, long a5, long a6)
// {
// 	unsigned long ret;
// 	register long r10 __asm__("r10") = a4;
// 	register long r8 __asm__("r8") = a5;
// 	register long r9 __asm__("r9") = a6;
// 	__asm__ __volatile__("syscall" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2),
// 												 "d"(a3), "r"(r10), "r"(r8), "r"(r9) : "rcx", "r11", "memory");
// 	return ret;
// }
