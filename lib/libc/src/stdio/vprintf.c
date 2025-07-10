#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

static void print_string(FILE *stream, const char *s)
{
	while (*s)
		stream->write(stream, s++, 1);
}

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

static void print_double(FILE *stream, double value)
{
	char buf[DOUBLE_BUF_SIZE];
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

	double point_number = (value - (int)value) * 1000000;
	int point_number_int = (int)point_number;

	while (point_number_int >= 1)
	{
		buf[i++] = '0' + (point_number_int % 10);
		point_number_int /= 10;
	}

	buf[i++] = '.';

	while (value >= 1)
	{
		buf[i++] = '0' + ((int)value % 10);
		value /= 10;
	}

	if (negative)
		buf[i++] = '-';

	for (int j = i - 1; j >= 0; j--)
		stream->write(stream, &buf[j], 1);
}

// static const char xdigits[16] = {"0123456789ABCDEF"};
static const char xdigits[16] = {"0123456789abcdef"};

static void print_hex(FILE *stream, uintptr_t value)
{
	char buf[HEX_BUF_SIZE + 1];
	int i = 0;

	if (value == 0)
	{
		char zero = '0';
		stream->write(stream, &zero, 1);
		return;
	}

	while (value > 0)
	{
		buf[i++] = xdigits[value % 16];
		value /= 16;
	}

	for (int j = i - 1; j >= 0; j--)
		stream->write(stream, &buf[j], 1);
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
			case 'f':
			{
				double val = va_arg(args, double);
				print_double(stream, val);
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
