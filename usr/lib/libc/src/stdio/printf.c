#include <stdio.h>

#include <stdint.h>

int printf(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	int ret = vfprintf(stdout, format, args);
	va_end(args);
	return ret;
}
