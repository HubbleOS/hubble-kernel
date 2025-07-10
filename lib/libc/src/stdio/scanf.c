#include <stdio.h>

int scanf(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	int ret = vscanf(format, args);
	va_end(args);
	return ret;
}