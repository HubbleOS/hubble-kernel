/**
 * @file getc.c
 * @brief Read a character from a stream
 */

#include <stdio.h>

int getc(FILE *stream)
{
	if (!stream || !stream->read)
		return -1;

	int c = FILE_GETC_UNGETC(stream);
	if (c != -2)
		return c;

	static int buf_len = 0;
	static int buf_pos = 0;

	if (buf_pos >= buf_len)
	{
		buf_len = stream->read(stream, stream->buf, sizeof(stream->buf));
		buf_pos = 0;
		if (buf_len <= 0)
			return -1;
	}

	return (unsigned char)stream->buf[buf_pos++];
}
