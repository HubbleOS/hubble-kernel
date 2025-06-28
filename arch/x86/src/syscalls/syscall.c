#include <stdint.h>
#include <sys/syscall.h>
#include <sys/syscall_nums.h>

#include <stdbool.h>
#include <ctype.h>

#include "stddef.h"

#include "utils/font.h"

extern framebuffer_info_t *g_fb;
extern int cursor_x, cursor_y;

void backspace()
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
};

static const char scancode_shift_ascii[128] = {
	0, 27, '!', '@', '#', '$', '%', '^', '&', '*',	  // 0x00 - 0x09
	'(', ')', '_', '+', '\b',						  // Backspace
	'\t',											  // Tab
	'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', // 0x10 - 0x19
	'{', '}', '\n',									  // Enter
	0,												  // Control
	'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', // 0x1E - 0x27
	'"', '~', 0,									  // Left Shift
	'|', 'Z', 'X', 'C', 'V', 'B', 'N',				  // 0x2B - 0x31
	'M', '<', '>', '?', 0,							  // Right Shift
	'*', 0, ' ',									  // Space
};

typedef struct
{
	uint8_t scancode;
	bool extended;
	bool released;
	bool is_shift;
	bool is_ctrl;
	bool is_alt;
	bool is_caps_lock;
} key_event_t;

#define KEY_SHIFT_LEFT 0x2A
#define KEY_SHIFT_RIGHT 0x36
#define KEY_CAPS_LOCK 0x3A
#define KEY_CONTROL 0x1D
#define KEY_ALT 0x38

#define KEY_LEFT 0x4B
#define KEY_UP 0x48
#define KEY_RIGHT 0x4D
#define KEY_DOWN 0x50

static key_event_t read_key_event()
{
	static bool extended = false;
	static bool shift_pressed = false;
	static bool ctrl_pressed = false;
	static bool alt_pressed = false;
	static bool caps_lock_active = false;

	while (1)
	{
		uint8_t sc = kbd_read_scancode();
		uint8_t scancode = sc & 0x7F;
		bool released = sc & 0x80;

		if (sc == 0xE0)
		{
			extended = true;
			continue;
		}

		// Shift (left or right)
		if (scancode == 0x2A || scancode == 0x36)
		{
			shift_pressed = !released;
		}
		// Ctrl (left or right)
		else if ((scancode == 0x1D && !extended) || (scancode == 0x1D && extended))
		{
			ctrl_pressed = !released;
		}
		// Alt (left or right)
		else if ((scancode == 0x38 && !extended) || (scancode == 0x38 && extended))
		{
			alt_pressed = !released;
		}
		// Caps Lock toggle
		else if (scancode == 0x3A && !released)
		{
			caps_lock_active = !caps_lock_active;
		}

		key_event_t evt = {
			.scancode = scancode,
			.extended = extended,
			.released = released,
			.is_shift = shift_pressed,
			.is_ctrl = ctrl_pressed,
			.is_alt = alt_pressed,
			.is_caps_lock = caps_lock_active};

		extended = false;
		return evt;
	}
}

long sys_read(int fd, char *buffer, size_t len)
{
	if (fd != 0)
		return -1;

	size_t i = 0;
	while (i < len)
	{
		key_event_t evt = read_key_event();

		if (evt.released)
			continue;

		char c = 0;

		if (evt.extended)
		{
			switch (evt.scancode)
			{
			case KEY_LEFT:
				c = '<';
				break;
			case KEY_UP:
				c = '^';
				break;
			case KEY_RIGHT:
				c = '>';
				break;
			case KEY_DOWN:
				c = 'v';
				break;

			default:
				break;
			}
			// continue;
		}
		else
		{
			char base = scancode_to_ascii[evt.scancode];
			char shifted = scancode_shift_ascii[evt.scancode];

			bool is_letter = isalpha(base);
			bool upper = evt.is_shift ^ (evt.is_caps_lock && is_letter);

			c = upper ? shifted : base;
		}

		// if (c == 0)
		// continue;

		if (c == '\b')
		{
			if (i > 0)
			{
				i--;
				char backspace = '\b';
				sys_write(1, &backspace, 1);
			}
			continue;
		}

		buffer[i++] = c;
		sys_write(1, &c, 1);

		if (c == '\n')
			break;
	}
	return i;
}

long syscall_dispatcher(long n, long a1, long a2, long a3, long a4, long a5, long a6)
{
	(void)a4;
	(void)a5;
	(void)a6;

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