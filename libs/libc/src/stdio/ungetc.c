#include "stdio.h"

int ungetc(int c, FILE *stream)
{
	if (!stream || c == -1)
		return -1;

	FILE_SET_UNGETC(stream, c);
	return c;
}