// user_main.c
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include "libc.h"
#include <stdio.h>

#include <sys/syscall.h>

void *mmap_(uint64_t addr, size_t length, int prot, int flags,
	    int fd, uint64_t offset)
{
	return (uint64_t *)syscall6(3, addr, length, prot, flags, fd, offset);
}

int read_file(int fd, void *buf, size_t size) { return syscall3(2, fd, (long)buf, size); }

int write_file(int fd, void *buf, size_t size) { return syscall3(1, fd, (long)buf, size); }

int lseek(int fd, uint64_t offset, int whence) { return syscall3(8, fd, offset, whence); }

int spawn(void *entry_point, void *arg, uint32_t priority) { return syscall3(6, (long)entry_point, (long)arg, priority); }

typedef struct
{
	int32_t x, y;
	bool left, right, middle; // current held state
	bool left_clicked;	  // set on press, you clear it after handling
	bool right_clicked;
} mouse_t;

uint32_t open(const char *path, int flags) { return syscall2(4, (long)path, flags); }

void test(void);

void _start(void)
{
	libc_init();
	printf("Hello from user space 2!\n");
	int pid = spawn(test, NULL, 0);
	while (1)
	{
		;
	}
}
void test(void)
{
	printf("Test task 2!\n");
	char c;
	char buf[128];
	int pos = 0;
	int kbd_file = open("/dev/kbd", 0);
	int pipe_file = open("/pipe/test", 0);
	while (1)
	{
		;
		read_file(kbd_file, &c, 1);
		buf[pos++] = c;
		lseek(pipe_file, 0, 0);
		write_file(pipe_file, buf, pos);
		pos = 0;
		// printf("%s", buf);
		// printf("key 2: %c\n", c);
	};
	// printf("Test task!\n")
}
