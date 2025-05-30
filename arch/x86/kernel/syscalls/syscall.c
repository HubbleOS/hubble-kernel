#include <stdint.h>
#include "sys/syscall.h"
#include "sys/syscall_numbers.h"

#include "stddef.h"

#include "utils/font.h"

extern framebuffer_info_t *g_fb;
extern int cursor_x, cursor_y;

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

// Упрощённая таблица сканкодов -> ASCII (без учёта Shift, Ctrl и т.п.)
static const char scancode_to_ascii[128] = {
	0, 27, '1', '2', '3', '4', '5', '6', '7', '8',	  // 0x00 - 0x09
	'9', '0', '-', '=', '\b',						  // Backspace 0x0E
	'\t',											  // Tab 0x0F
	'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', // 0x10 - 0x19
	'[', ']', '\n',									  // Enter key 0x1C
	0,												  // Control 0x1D
	'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', // 0x1E - 0x27
	'\'', '`', 0,									  // Left Shift 0x2A
	'\\', 'z', 'x', 'c', 'v', 'b', 'n',				  // 0x2B - 0x31
	'm', ',', '.', '/', 0,							  // Right Shift 0x36
	'*', 0, ' ',									  // Space 0x39
													  // Остальное 0 — необработанные клавиши
};

long sys_read(int fd, char *buffer, size_t len)
{
	int i = 0;

	while (i < len)
	{
		int c = -1;
		do
		{
			uint8_t scancode = kbd_read_scancode();

			if (scancode & 0x80) // отпускание клавиши
				continue;

			c = scancode_to_ascii[scancode];
			if (c == 0)
				continue;

		} while (c == -1);

		buffer[i++] = (char)c;
		putchar(c); // эхо-вывод

		if (c == '\n')
			break;
	}

	return i;
}

long syscall_dispatcher(long n, long a1, long a2, long a3, long a4, long a5, long a6)
{
	switch (n)
	{
	case SYS_WRITE:
		return sys_write((int)a1, (const char *)a2, (size_t)a3);

	case SYS_READ:
		return sys_read((int)a1, (char *)a2, (size_t)a3);

	default:
		return -1; // unknown syscall
	}
}
