#include "stdio.h"
#include "utils/font.h"
#include "utils/framebuffer.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "sys/syscall.h"
#include "sys/syscall_nums.h"

static FILE __stdout;
static FILE __stdin;
static FILE __stderr;

FILE *__stdoutp = &__stdout;
FILE *__stdinp = &__stdin;
FILE *__stderrp = &__stderr;

extern framebuffer_info_t *g_fb;
extern int cursor_x;
extern int cursor_y;

int fb_write(struct FILE *stream, const char *buffer, int len)
{
  (void)stream;
  return syscall_dispatcher(SYS_WRITE, 1, (long)buffer, len, 0, 0, 0);
}

int kb_read(FILE *stream, char *buffer, int len)
{
  (void)stream;
  return syscall_dispatcher(SYS_READ, 0, (long)buffer, len, 0, 0, 0);
}

void stdio_init()
{
  stdout->write = fb_write;
  stdout->read = NULL;
  stdout->device = NULL;

  stdin->read = kb_read;
  stdin->write = NULL;
  stdin->device = NULL;

  stdout->device = NULL;
  stdin->device = NULL;
  stderr->device = NULL;
}

int putc(int c, struct FILE *stream)
{
  char ch = c;
  if (stream && stream->write)
    return stream->write(stream, &ch, 1);
  return -1;
}

int putchar(int c) { return putc(c, stdout); }

static void print_string(FILE *stream, const char *s)
{
  while (*s)
    stream->write(stream, s++, 1);
}

#define INT_BUF_SIZE 12
#define DOUBLE_BUF_SIZE 24
#define HEX_BUF_SIZE 2 * sizeof(uintptr_t)

static void print_int(FILE *stream, int value)
{
  char buf[INT_BUF_SIZE];
  int i = 0;
  bool negative = false;

  if (value == 0)
  {
    char c = '0';
    stream->write(stream, &c, 1);
    return;
  }

  if (value < 0)
  {
    negative = true;
    value = -value;
  }

  while (value > 0)
  {
    buf[i++] = '0' + (value % 10);
    value /= 10;
  }

  if (negative)
    buf[i++] = '-';

  for (int j = i - 1; j >= 0; j--)
    stream->write(stream, &buf[j], 1);
}

static void print_double(FILE *stream, double value)
{
  char buf[DOUBLE_BUF_SIZE];
  int i = 0;
  bool negative = false;

  if (value == 0)
  {
    char c = '0';
    stream->write(stream, &c, 1);
    return;
  }

  if (value < 0)
  {
    negative = true;
    value = -value;
  }

  double point_number = (value - (int)value) * 1000000;
  int point_number_int = (int)point_number;

  while (point_number_int >= 1)
  {
    buf[i++] = '0' + (point_number_int % 10);
    point_number_int /= 10;
  }

  buf[i++] = '.';

  while (value >= 1)
  {
    buf[i++] = '0' + ((int)value % 10);
    value /= 10;
  }

  if (negative)
    buf[i++] = '-';

  for (int j = i - 1; j >= 0; j--)
    stream->write(stream, &buf[j], 1);
}

static void print_hex(FILE *stream, uintptr_t value)
{
  char buf[HEX_BUF_SIZE + 1];
  int i = 0;

  if (value == 0)
  {
    char zero = '0';
    stream->write(stream, &zero, 1);
    return;
  }

  while (value > 0)
  {
    buf[i++] = "0123456789abcdef"[value % 16];
    value /= 16;
  }

  // Выводим цифры в обратном порядке
  for (int j = i - 1; j >= 0; j--)
    stream->write(stream, &buf[j], 1);
}

int vfprintf(FILE *stream, const char *format, va_list args)
{
  int written = 0;
  while (*format)
  {
    if (*format == '%')
    {
      format++;
      switch (*format)
      {
      case 'd':
      case 'i':
      {
        int val = va_arg(args, int);
        print_int(stream, val);
        break;
      }
      case 's':
      {
        const char *str = va_arg(args, const char *);
        print_string(stream, str);
        break;
      }
      case 'c':
      {
        char c = (char)va_arg(args, int);
        stream->write(stream, &c, 1);
        break;
      }
      case 'p':
      {
        void *ptr = va_arg(args, void *);
        print_string(stream, "0x");
        print_hex(stream, (uintptr_t)ptr);
        break;
      }
      case 'f':
      {
        double val = va_arg(args, double);
        print_double(stream, val);
        break;
      }
      case '%':
      {
        char c = '%';
        stream->write(stream, &c, 1);
        break;
      }
      default:
      {
        stream->write(stream, format, 1);
        break;
      }
      }
    }
    else
    {
      stream->write(stream, format, 1);
    }
    format++;
  }
  return written;
}

int fprintf(FILE *stream, const char *format, ...)
{
  va_list args;
  va_start(args, format);
  int ret = vfprintf(stream, format, args);
  va_end(args);
  return ret;
}

int printf(const char *format, ...)
{
  va_list args;
  va_start(args, format);
  int ret = vfprintf(stdout, format, args);
  va_end(args);
  return ret;
}

// input

static int ungetc_buffer = -1;

int ungetc(int c, FILE *stream)
{
  if (stream != stdin)
    return -1;

  ungetc_buffer = c;
  return c;
}

int getc(FILE *stream)
{
  if (stream && stream->read)
  {
    static char input_buf[128];
    static int buf_len = 0;
    static int buf_pos = 0;

    if (stream == stdin && ungetc_buffer != -1)
    {
      int c = ungetc_buffer;
      ungetc_buffer = -1;
      return c;
    }

    if (buf_pos >= buf_len)
    {
      buf_len = stream->read(stream, input_buf, sizeof(input_buf));
      buf_pos = 0;

      if (buf_len <= 0)
        return -1;
    }

    return (unsigned char)input_buf[buf_pos++];
  }

  return -1;
}

char *fgets(char *s, int size, FILE *stream)
{
  int c;
  int i = 0;
  while ((c = getc(stream)) != EOF)
  {
    if (c == '\n')
      break;
    if (i < size - 1)
      s[i++] = c;
  }
  s[i] = '\0';
  return s;
}

char *gets(char *s) { return fgets(s, 256, stdin); }

int getchar(void) { return getc(stdin); }

#include <ctype.h>

static void skip_whitespace()
{
  int c;
  do
  {
    c = getchar();
  } while (isspace(c));
  ungetc(c, stdin);
}

int vscanf(const char *format, va_list args)
{
  int assigned = 0;
  while (*format)
  {
    if (*format == '%')
    {
      format++;
      switch (*format)
      {
      case 'd':
      {
        int *ptr = va_arg(args, int *);
        int num = 0;
        int sign = 1;

        skip_whitespace();

        int c = getchar();
        if (c == '-')
        {
          sign = -1;
          c = getchar();
        }

        if (!isdigit(c))
          return assigned;

        do
        {
          num = num * 10 + (c - '0');
          c = getchar();
        } while (isdigit(c));

        *ptr = num * sign;
        assigned++;
        break;
      }
      case 's':
      {
        char *str = va_arg(args, char *);
        skip_whitespace();
        int c;
        while ((c = getchar()) != EOF && !isspace(c))
          *str++ = (char)c;
        *str = '\0';
        assigned++;
        break;
      }
      case 'c':
      {
        char *ch = va_arg(args, char *);
        int c = getchar();
        if (c == EOF)
          return assigned;
        *ch = (char)c;
        assigned++;
        break;
      }
      default:
        break;
      }
    }
    else
    {
      format++;
    }
  }

  return assigned;
}

int scanf(const char *format, ...)
{
  va_list args;
  va_start(args, format);
  int ret = vscanf(format, args);
  va_end(args);
  return ret;
}

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
