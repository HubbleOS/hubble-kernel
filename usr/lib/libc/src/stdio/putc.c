#include <stdio.h>

int putc(int c, FILE *stream)
{
	if (!stream)
		return EOF;
	stream->buf[stream->buf_pos++] = (char)c;

	if (stream->buf_pos >= FILE_BUFSIZE || c == '\n')
	{
		int written = stream->write(stream, stream->buf, stream->buf_pos);
		stream->buf_pos = 0;
		if (written != FILE_BUFSIZE)
			return EOF;
	}
	return (unsigned char)c;
}
