#include <stdio.h>

int getc(FILE *stream)
{
	if (!stream || !stream->read)
		return -1;

	int c = FILE_GETC_UNGETC(stream);
	if (c != -2) // -2: there is no symbol
		return c;

	// static char input_buf[128];
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
