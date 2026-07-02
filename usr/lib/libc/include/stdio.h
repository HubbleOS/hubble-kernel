/**
 * @file stdio.h
 * @brief Standard input/output declarations
 */

#pragma once

#include <_cheader.h>
#include <stdarg.h>

#define FILE_HAS_UNGETC(stream) ((stream)->has_ungetc)
#define FILE_GETC_UNGETC(stream)                                               \
  ((stream)->has_ungetc ? ((stream)->has_ungetc = 0, (stream)->ungetc_buf) : -2)
#define FILE_SET_UNGETC(stream, c)                                             \
  do {                                                                         \
    (stream)->ungetc_buf = (c);                                                \
    (stream)->has_ungetc = 1;                                                  \
  } while (0)

#define FILE_BUFSIZE 128

#define INT_BUF_SIZE 12
#define DOUBLE_BUF_SIZE 24
#define HEX_BUF_SIZE 2 * sizeof(uintptr_t)

_Begin_C_Header;

/** @brief File stream structure */
typedef struct FILE {
  void *device;
  int (*write)(struct FILE *stream, const char *buffer, int len);
  int (*read)(struct FILE *stream, char *buffer, int len);

  int ungetc_buf;
  int has_ungetc;

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

/** @brief Formatted print to a stream */
int fprintf(FILE *, const char *, ...);

/** @brief Formatted print to a stream (va_list) */
int vfprintf(FILE *, const char *, va_list);

/** @brief Formatted print to stdout */
int printf(const char *, ...);

/** @brief Write a character to a stream */
int putc(int, FILE *);

/** @brief Write a character to stdout */
int putchar(int);

/** @brief Formatted input from a stream */
int fscanf(FILE *, const char *, ...);

/** @brief Formatted input from stdin */
int scanf(const char *, ...);

/** @brief Formatted input from stdin (va_list) */
int vscanf(const char *, va_list);

/** @brief Read a character from a stream */
int getc(FILE *);

/** @brief Read a line from a stream */
char *fgets(char *, int, FILE *);

/** @brief Read a line from stdin */
char *gets(char *);

/** @brief Read a character from stdin */
int getchar(void);

/** @brief Push back a character to a stream */
int ungetc(int c, FILE *);

_End_C_Header;
