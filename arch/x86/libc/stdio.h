#pragma once
#include <stdarg.h>

typedef struct FILE
{
	void *device;
	int (*write)(struct FILE *stream, const char *buffer, int len);
	int (*read)(struct FILE *stream, char *buffer, int len);
} FILE;

extern FILE *__stdoutp;
extern FILE *__stdinp;
extern FILE *__stderrp;

#define EOF (-1)

#define stdout __stdoutp
#define stdin __stdinp
#define stderr __stderrp

#ifdef __cplusplus
extern "C"
{
#endif

	// output
	int fprintf(FILE *stream, const char *format, ...);
	int vfprintf(FILE *stream, const char *format, va_list args);
	// int vsprintf(char *buffer, const char *format, va_list args);
	int printf(const char *format, ...);

	int putc(int c, FILE *stream);
	void putchar(int c);

	// input
	int fscanf(FILE *stream, const char *format, ...);
	int scanf(const char *format, ...);
	int vscanf(const char *format, va_list args);
	int getc(FILE *stream);
	int getchar();

	int ungetc(int c, FILE *stream);

	void stdio_init();

#ifdef __cplusplus
}
#endif
