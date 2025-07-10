#include <stdio.h>

int putc(int c, struct FILE *stream)
{
	char ch = c;
	if (stream && stream->write)
		return stream->write(stream, &ch, 1);
	return -1;
}
