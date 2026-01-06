#pragma once

#include <_cheader.h>
#include <stdarg.h>

#define FILE_HAS_UNGETC(stream) ((stream)->has_ungetc)
#define FILE_GETC_UNGETC(stream) ((stream)->has_ungetc ? ((stream)->has_ungetc = 0, (stream)->ungetc_buf) : -2)
#define FILE_SET_UNGETC(stream, c)          \
	do                                  \
	{                                   \
		(stream)->ungetc_buf = (c); \
		(stream)->has_ungetc = 1;   \
	} while (0)

#define FILE_BUFSIZE 128

#define INT_BUF_SIZE 12
#define DOUBLE_BUF_SIZE 24
#define HEX_BUF_SIZE 2 * sizeof(uintptr_t)

_Begin_C_Header;

typedef struct FILE
{
	void *device;
	int (*write)(struct FILE *stream, const char *buffer, int len);
	int (*read)(struct FILE *stream, char *buffer, int len);

	// // ungetc support
	int ungetc_buf;
	int has_ungetc;

	// // buffering
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
int fprintf(FILE *, const char *, ...);
int vfprintf(FILE *, const char *, va_list);
// int vsprintf(char *buffer, const char *, va_list );
int printf(const char *, ...);

int putc(int, FILE *);
int putchar(int);

// input
int fscanf(FILE *, const char *, ...);
int scanf(const char *, ...);
int vscanf(const char *, va_list);
int getc(FILE *);
char *fgets(char *, int, FILE *);
char *gets(char *);
int getchar(void);

int ungetc(int c, FILE *);

_End_C_Header;
