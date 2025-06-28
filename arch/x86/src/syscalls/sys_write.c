#include <stddef.h>

#include <sys/syscall.h>
#include <sys/syscall_nums.h>

#include "utils/font.h"

extern framebuffer_info_t *g_fb;
extern int cursor_x, cursor_y;

void backspace(void)
{
	if (cursor_x >= CHAR_WIDTH)
	{
		cursor_x -= CHAR_WIDTH;
	}
	else if (cursor_y >= CHAR_HEIGHT)
	{
		cursor_y -= CHAR_HEIGHT;
		cursor_x = g_fb->width - CHAR_WIDTH;
	}
	else
	{
		return;
	}

	// draw_char(g_fb, ' ', cursor_x, cursor_y);
	clear_char_area(g_fb, cursor_x, cursor_y);
}

long sys_write(int fd, const char *buffer, size_t len)
{
	if (fd != 1) // stdout
		return -1;

	if (!g_fb)
		return -1;

	for (size_t i = 0; i < len; i++)
	{
		char c = buffer[i];
		if (c == '\n')
		{
			cursor_x = 0;
			cursor_y += CHAR_HEIGHT;
			continue;
		}
		if (c == '\b')
		{
			backspace();
			continue;
		}

		draw_char(g_fb, c, cursor_x, cursor_y);
		cursor_x += CHAR_WIDTH;

		if ((unsigned int)cursor_x + CHAR_WIDTH > g_fb->width)
		{
			cursor_x = 0;
			cursor_y += CHAR_HEIGHT;
		}
	}
	return len;
}
