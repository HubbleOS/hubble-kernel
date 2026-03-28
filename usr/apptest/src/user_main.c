// user_main.c
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include "libc.h"
#include <stdio.h>

#include <sys/syscall.h>

#include <gui/core/screen/screen.h>
#include <gui/core/compositor/compositor.h>

#include <gui/dev/mouse/mouse.h>

#include <gui/ui/window/window.h>
#include <gui/ui/сursor/cursor.h>
#include <gui/ui/background/background.h>

void *mmap_(uint64_t addr, size_t length, int prot, int flags,
	    int fd, uint64_t offset)
{
	return (uint64_t *)syscall6(3, addr, length, prot, flags, fd, offset);
}

int read_file(int fd, void *buf, size_t size) { return syscall3(0, fd, (long)buf, size); }

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
	while (1)
	{
		;
	}
}
void test(void)
{
	printf("Test task!\n");
	char c;
	int kbd_file = open("/dev/kbd", 0);
	while (1)
	{
		read_file(kbd_file, &c, 1);
		printf("key: %c\n", c);
	}
	// printf("Test task!\n")
}
