#ifndef _STDIO_H_
#define _STDIO_H_

#define FILE_HAS_UNGETC(stream) ((stream)->has_ungetc)
#define FILE_GETC_UNGETC(stream) ((stream)->has_ungetc ? ((stream)->has_ungetc = 0, (stream)->ungetc_buf) : -2)
#define FILE_SET_UNGETC(stream, c)  \
	do                              \
	{                               \
		(stream)->ungetc_buf = (c); \
		(stream)->has_ungetc = 1;   \
	} while (0)

#define FILE_BUFSIZE 128

#define INT_BUF_SIZE 12
#define DOUBLE_BUF_SIZE 24
#define HEX_BUF_SIZE 2 * sizeof(uintptr_t)

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdarg.h>

	typedef struct FILE
	{
		void *device;
		int (*write)(struct FILE *stream, const char *buffer, int len);
		int (*read)(struct FILE *stream, char *buffer, int len);

		// ungetc support
		int ungetc_buf;
		int has_ungetc;

		// buffering
		char buf[FILE_BUFSIZE];
		int buf_len;
		int buf_pos;
	} FILE;

	extern FILE *__stdoutp;
	extern FILE *__stdinp;
	extern FILE *__stderrp;

#define EOF (-1)

#define stdout __stdoutp
#define stdin __stdinp
#define stderr __stderrp

	// output
	int fprintf(FILE *stream, const char *format, ...);
	int vfprintf(FILE *stream, const char *format, va_list args);
	// int vsprintf(char *buffer, const char *format, va_list args);
	int printf(const char *format, ...);

	int putc(int c, FILE *stream);
	int putchar(int c);

	// input
	int fscanf(FILE *stream, const char *format, ...);
	int scanf(const char *format, ...);
	int vscanf(const char *format, va_list args);
	int getc(FILE *stream);
	char *fgets(char *s, int size, FILE *stream);
	char *gets(char *s);
	int getchar(void);

	int ungetc(int c, FILE *stream);

	// void stdio_init();
	void stdio_init(FILE *in, FILE *out, FILE *err);

#ifdef __cplusplus
}
#endif

#endif