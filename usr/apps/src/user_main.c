// user_main.c
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include "libc.h"
#include <stdio.h>

#include <sys/syscall.h>

#include <libgui/core.h>
#include <libgui/ui.h>

#include <dev/mouse/mouse.h>

void *mmap_(uint64_t addr, size_t length, int prot, int flags,
	    int fd, uint64_t offset)
{
	return (uint64_t *)syscall6(3, addr, length, prot, flags, fd, offset);
}

int read_file(int fd, void *buf, size_t size) { return syscall3(SYS_read, fd, (long)buf, size); }

int spawn(void *entry_point, void *arg, uint32_t priority) { return syscall3(SYS_spawn, (long)entry_point, (long)arg, priority); }
int spawn_file(const char *path, void *arg, uint32_t priority) { return syscall3(7, (long)path, (long)arg, priority); }
typedef struct
{
	int32_t x, y;
	bool left, right, middle; // current held state
	bool left_clicked;	  // set on press, you clear it after handling
	bool right_clicked;
} mouse_t;

uint32_t open(const char *path, int flags) { return syscall2(SYS_open, (long)path, flags); }

void test(void);

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

	framebuffer_info_t fb_info;

	fb_info.base = fb_test;
	fb_info.width = width;
	fb_info.height = height;
	fb_info.pitch = pitch;
	fb_info.bpp = 32;

	screen_init(&fb_info);
	compositor_init();
	background_create(rgb(20, 20, 20));

	cursor_t *cursor = cursor_create(16, 16, rgb(0, 0, 0), rgb(255, 255, 255));
	window_t *win = window_create(0, 0, 400, 300);

	int pid = spawn(test, NULL, 0);
	int pid_file = spawn_file("/usr/bin/user1.elf", NULL, 0);
	while (1)
	{

		mouse_update(mouse->x, mouse->y, mouse->left);

		if (cursor->surface->x != mouse->x || cursor->surface->y != mouse->y)
			cursor_move(cursor, mouse->x, mouse->y);

		if (g_mouse.drag_obj)
		{
			int drag_x = g_mouse.x - g_mouse.drag_offset_x;
			int drag_y = g_mouse.y - g_mouse.drag_offset_y;
			if (g_mouse.drag_el)
				object_move_element(g_mouse.drag_obj, g_mouse.drag_el,
						    drag_x - g_mouse.drag_obj->x,
						    drag_y - g_mouse.drag_obj->y);
			else
			{
				compositor_move_object(g_mouse.drag_obj, drag_x, drag_y);
				compositor_bring_to_front(g_mouse.drag_obj, LAYER_WINDOWS);
			}
		}

		compositor_render();
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
		printf("key 1: %c\n", c);
	}
	// printf("Test task!\n");
}
