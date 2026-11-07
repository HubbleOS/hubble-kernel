#include <hubble/ctype.h>
#include <hubble/stdlib.h>
#include <stdbool.h>
#include <stddef.h>

unsigned long strtoul(const char *s, char **end, int base) {
  const char *p = s;
  while (isspace((unsigned char)*p))
    p++;
  bool neg = false;
  if (*p == '+' || *p == '-')
    neg = *p++ == '-';

  if ((base == 0 || base == 16) && p[0] == '0' && (p[1] == 'x' || p[1] == 'X') &&
      isxdigit((unsigned char)p[2])) {
    p += 2;
    base = 16;
  } else if (base == 0) {
    base = *p == '0' ? 8 : 10;
  }

  unsigned long n = 0;
  const char *digits = p;
  for (;; p++) {
    int c = (unsigned char)*p, d;
    if (isdigit(c))
      d = c - '0';
    else if (isalpha(c))
      d = tolower(c) - 'a' + 10;
    else
      break;
    if (d >= base)
      break;
    n = n * base + d;
  }
  if (end)
    *end = (char *)(p == digits ? s : p);
  return neg ? -n : n;
}

long strtol(const char *s, char **end, int base) {
  return (long)strtoul(s, end, base);
}

int atoi(const char *s) { return (int)strtol(s, NULL, 10); }
