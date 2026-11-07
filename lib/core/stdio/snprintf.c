#include <hubble/stdio.h>
#include <hubble/string.h>
#include <stdbool.h>
#include <stdint.h>

/* Output that counts everything but stores only what fits. */
typedef struct {
  char *buf;
  size_t size, len;
} out_t;

static void put(out_t *o, char c) {
  if (o->len + 1 < o->size)
    o->buf[o->len] = c;
  o->len++;
}

static void pad(out_t *o, char c, int n) {
  while (n-- > 0)
    put(o, c);
}

int vsnprintf(char *buf, size_t size, const char *fmt, va_list args) {
  out_t o = {buf, size, 0};

  for (; *fmt; fmt++) {
    if (*fmt != '%') {
      put(&o, *fmt);
      continue;
    }

    bool left = false, zero = false, plus = false, space = false;
    for (;; fmt++) {
      if (fmt[1] == '-')
        left = true;
      else if (fmt[1] == '0')
        zero = true;
      else if (fmt[1] == '+')
        plus = true;
      else if (fmt[1] == ' ')
        space = true;
      else if (fmt[1] != '#')
        break;
    }
    fmt++;

    int width = 0, prec = -1;
    if (*fmt == '*') {
      width = va_arg(args, int);
      if (width < 0)
        left = true, width = -width;
      fmt++;
    } else {
      while (*fmt >= '0' && *fmt <= '9')
        width = width * 10 + (*fmt++ - '0');
    }
    if (*fmt == '.') {
      fmt++;
      prec = 0;
      if (*fmt == '*') {
        prec = va_arg(args, int);
        fmt++;
      } else {
        while (*fmt >= '0' && *fmt <= '9')
          prec = prec * 10 + (*fmt++ - '0');
      }
    }

    int lng = 0; /* -2 hh, -1 h, 1 l / z / t / j, 2 ll */
    for (;; fmt++) {
      if (*fmt == 'h')
        lng--;
      else if (*fmt == 'l')
        lng++;
      else if (*fmt == 'z' || *fmt == 't' || *fmt == 'j')
        lng = 1;
      else
        break;
    }

    char tmp[24];
    const char *s = tmp;
    int n = 0;
    char sign = 0;
    bool number = true;

    switch (*fmt) {
    case 'd':
    case 'i': {
      long long v = lng >= 2   ? va_arg(args, long long)
                    : lng == 1 ? va_arg(args, long)
                               : va_arg(args, int);
      if (lng == -1)
        v = (short)v;
      else if (lng <= -2)
        v = (signed char)v;
      unsigned long long u =
          v < 0 ? -(unsigned long long)v : (unsigned long long)v;
      sign = v < 0 ? '-' : plus ? '+' : space ? ' ' : 0;
      char *p = tmp + sizeof(tmp);
      do
        *--p = '0' + u % 10;
      while (u /= 10);
      s = p;
      n = tmp + sizeof(tmp) - p;
      break;
    }
    case 'u':
    case 'x':
    case 'X':
    case 'o':
    case 'p': {
      unsigned long long u;
      if (*fmt == 'p')
        u = (uintptr_t)va_arg(args, void *);
      else
        u = lng >= 2   ? va_arg(args, unsigned long long)
            : lng == 1 ? va_arg(args, unsigned long)
                       : va_arg(args, unsigned int);
      if (lng == -1)
        u = (unsigned short)u;
      else if (lng <= -2)
        u = (unsigned char)u;
      int base = *fmt == 'o' ? 8 : *fmt == 'u' ? 10 : 16;
      const char *dig =
          *fmt == 'X' ? "0123456789ABCDEF" : "0123456789abcdef";
      char *p = tmp + sizeof(tmp);
      do
        *--p = dig[u % base];
      while (u /= base);
      if (*fmt == 'p') {
        *--p = 'x';
        *--p = '0';
      }
      s = p;
      n = tmp + sizeof(tmp) - p;
      break;
    }
    case 'c':
      tmp[0] = (char)va_arg(args, int);
      n = 1;
      number = false;
      break;
    case 's':
      s = va_arg(args, const char *);
      if (!s)
        s = "(null)";
      n = (int)strlen(s);
      if (prec >= 0 && n > prec)
        n = prec;
      number = false;
      break;
    case '%':
      tmp[0] = '%';
      n = 1;
      number = false;
      break;
    default: /* unknown: print it as it is */
      put(&o, '%');
      if (!*fmt)
        goto done;
      put(&o, *fmt);
      continue;
    }

    int zeros = number && prec > n ? prec - n : 0;
    int total = n + zeros + (sign ? 1 : 0);
    if (number && zero && !left && prec < 0 && width > total) {
      zeros += width - total;
      total = width;
    }
    if (!left)
      pad(&o, ' ', width - total);
    if (sign)
      put(&o, sign);
    pad(&o, '0', zeros);
    for (int i = 0; i < n; i++)
      put(&o, s[i]);
    if (left)
      pad(&o, ' ', width - total);
  }
done:
  if (size)
    buf[o.len < size ? o.len : size - 1] = '\0';
  return (int)o.len;
}

int snprintf(char *buf, size_t size, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  int n = vsnprintf(buf, size, fmt, args);
  va_end(args);
  return n;
}
