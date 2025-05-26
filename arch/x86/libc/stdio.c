#include "stdio.h"
#include "utils/framebuffer.h"
#include "utils/font.h"
#include <stddef.h>
#include <stdarg.h>

static FILE __stdout;
static FILE __stdin;
static FILE __stderr;

FILE *__stdoutp = &__stdout;
FILE *__stdinp = &__stdin;
FILE *__stderrp = &__stderr;

extern framebuffer_info_t *g_fb;
extern int cursor_x;
extern int cursor_y;

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

int fb_write(struct FILE *stream, const char *buffer, int len)
{
	(void)stream;
	if (!g_fb)
		return -1;

	for (int i = 0; i < len; i++)
	{
		char c = buffer[i];
		if (c == '\n')
		{
			cursor_x = 0;
			cursor_y += CHAR_HEIGHT * 2;
			continue;
		}
		draw_char(g_fb, c, cursor_x, cursor_y);
		cursor_x += CHAR_WIDTH;

		if ((unsigned int)cursor_x + CHAR_WIDTH > g_fb->width)
		{
			cursor_x = 0;
			cursor_y += CHAR_HEIGHT * 2;
		}
	}

	return len;
}

int kb_read(FILE *stream, char *buffer, int len)
{
	(void)stream;
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

void stdio_init()
{
	stdout->write = fb_write;
	stdout->read = NULL;
	stdout->device = NULL;

	stdin->read = kb_read;
	stdin->write = NULL;
	stdin->device = NULL;

	stdout->device = NULL;
	stdin->device = NULL;
	stderr->device = NULL;
}

int putc(int c, struct FILE *stream)
{
	char ch = c;
	if (stream && stream->write)
		return stream->write(stream, &ch, 1);
	return -1;
}

int putchar(int c) { return putc(c, stdout); }

#include <stdbool.h>

static void print_string(FILE *stream, const char *s)
{
	while (*s)
		stream->write(stream, s++, 1);
}

#define INT_BUF_SIZE 12

static void print_int(FILE *stream, int value)
{
	char buf[INT_BUF_SIZE];
	int i = 0;
	bool negative = false;

	if (value == 0)
	{
		char c = '0';
		stream->write(stream, &c, 1);
		return;
	}

	if (value < 0)
	{
		negative = true;
		value = -value;
	}

	while (value > 0)
	{
		buf[i++] = '0' + (value % 10);
		value /= 10;
	}

	if (negative)
		buf[i++] = '-';

	for (int j = i - 1; j >= 0; j--)
		stream->write(stream, &buf[j], 1);
}

static void print_hex(FILE *stream, uintptr_t value)
{
	char buf[2 * sizeof(uintptr_t) + 1];
	int i = 0;

	if (value == 0)
	{
		char zero = '0';
		stream->write(stream, &zero, 1);
		return;
	}

	while (value > 0)
	{
		buf[i++] = "0123456789abcdef"[value % 16];
		value /= 16;
	}

	// Выводим цифры в обратном порядке
	for (int j = i - 1; j >= 0; j--)
	{
		stream->write(stream, &buf[j], 1);
	}
}

int vfprintf(FILE *stream, const char *format, va_list args)
{
	int written = 0;
	while (*format)
	{
		if (*format == '%')
		{
			format++;
			switch (*format)
			{
			case 'd':
			case 'i':
			{
				int val = va_arg(args, int);
				print_int(stream, val);
				break;
			}
			case 's':
			{
				const char *str = va_arg(args, const char *);
				print_string(stream, str);
				break;
			}
			case 'c':
			{
				char c = (char)va_arg(args, int);
				stream->write(stream, &c, 1);
				break;
			}
			case 'p':
			{
				void *ptr = va_arg(args, void *);
				print_string(stream, "0x");
				print_hex(stream, (uintptr_t)ptr);
				break;
			}
			case '%':
			{
				char c = '%';
				stream->write(stream, &c, 1);
				break;
			}
			default:
			{
				stream->write(stream, format, 1);
				break;
			}
			}
		}
		else
		{
			stream->write(stream, format, 1);
		}
		format++;
	}
	return written;
}

int fprintf(FILE *stream, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	int ret = vfprintf(stream, format, args);
	va_end(args);
	return ret;
}

int printf(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	int ret = vfprintf(stdout, format, args);
	va_end(args);
	return ret;
}

// input

static int ungetc_buffer = -1;

int ungetc(int c, FILE *stream)
{
	if (stream != stdin)
		return -1;

	ungetc_buffer = c;
	return c;
}

int getc(FILE *stream)
{
	if (stream && stream->read)
	{
		static char input_buf[128];
		static int buf_len = 0;
		static int buf_pos = 0;

		if (stream == stdin && ungetc_buffer != -1)
		{
			int c = ungetc_buffer;
			ungetc_buffer = -1;
			return c;
		}

		if (buf_pos >= buf_len)
		{
			buf_len = stream->read(stream, input_buf, sizeof(input_buf));
			buf_pos = 0;

			if (buf_len <= 0)
				return -1;
		}

		return (unsigned char)input_buf[buf_pos++];
	}

	return -1;
}

char *fgets(char *s, int size, FILE *stream)
{
	int c;
	int i = 0;
	while ((c = getc(stream)) != EOF)
	{
		if (c == '\n')
			break;
		if (i < size - 1)
			s[i++] = c;
	}
	s[i] = '\0';
	return s;
}

char *gets(char *s) { return fgets(s, 256, stdin); }

int getchar(void) { return getc(stdin); }

#include <ctype.h>

static void skip_whitespace()
{
	int c;
	do
	{
		c = getchar();
	} while (isspace(c));
	ungetc(c, stdin);
}

int vscanf(const char *format, va_list args)
{
	int assigned = 0;
	while (*format)
	{
		if (*format == '%')
		{
			format++;
			switch (*format)
			{
			case 'd':
			{
				int *ptr = va_arg(args, int *);
				int num = 0;
				int sign = 1;

				skip_whitespace();

				int c = getchar();
				if (c == '-')
				{
					sign = -1;
					c = getchar();
				}

				if (!isdigit(c))
					return assigned;

				do
				{
					num = num * 10 + (c - '0');
					c = getchar();
				} while (isdigit(c));

				*ptr = num * sign;
				assigned++;
				break;
			}
			case 's':
			{
				char *str = va_arg(args, char *);
				skip_whitespace();
				int c;
				while ((c = getchar()) != EOF && !isspace(c))
					*str++ = (char)c;
				*str = '\0';
				assigned++;
				break;
			}
			case 'c':
			{
				char *ch = va_arg(args, char *);
				int c = getchar();
				if (c == EOF)
					return assigned;
				*ch = (char)c;
				assigned++;
				break;
			}
			default:
				break;
			}
		}
		else
		{
			format++;
		}
	}

	return assigned;
}

int scanf(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	int ret = vscanf(format, args);
	va_end(args);
	return ret;
}

int fscanf(FILE *stream, const char *format, ...)
{
	FILE *old_stdin = stdin;
	stdin = stream;

	va_list args;
	va_start(args, format);
	int ret = vscanf(format, args);
	va_end(args);

	stdin = old_stdin;
	return ret;
}
