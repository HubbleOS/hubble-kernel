/**
 * @file fscanf.c
 * @brief Formatted input from a stream
 */

#include <stdio.h>

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
