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

int read_file(int fd, void *buf, size_t size) { return syscall3(6, fd, (long)buf, size); }

typedef struct
{
	int32_t x, y;
	bool left, right, middle; // current held state
	bool left_clicked;	  // set on press, you clear it after handling
	bool right_clicked;
} mouse_t;

uint32_t open(const char *path, int flags) { return syscall2(4, (long)path, flags); }

void _start(void)
{
	libc_init();
	printf("Hello from user space!\n");

	int fb_file = open("/dev/fb0", 0);
	uint32_t *fb_test = (uint32_t *)mmap_(0, 800 * 5120, 3, 1, fb_file, 0);

	int mouse_file = open("/dev/mouse", 0);
	mouse_t *mouse = (mouse_t *)mmap_(0, sizeof(mouse_t), 3, 1, mouse_file, 0);

	int width = 1280;
	int height = 800;
	int pitch = 5120;

	for (int y = 0; y < height; y++)
		for (int x = 0; x < width; x++)
			fb_test[y * (pitch / 4) + x] = 0xFFFF0000;

	int x = 0, y = 0;

	const char *test = "test\n";
	syscall3(SYS_write, 1, (long)test, strlen(test));

	putchar('a');
	putchar('b');
	printf("hello %s\n", "world");
	printf("num: %d\n", 42);
	printf("flt: %f\n", 1.0);

	while (1)
	{
		mouse_t state;
		read_file(mouse_file, &state, sizeof(mouse_t));
		if (state.x != x || state.y != y)
		{
			x = state.x;
			y = state.y;
			printf("x: %d y: %d\n", x, y);
		}
	}
}
