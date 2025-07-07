#pragma once

#include <_cheader.h>

_Begin_C_Header;

#include <stddef.h>

// double atof(const char *nptr);
// int atoi(const char *nptr);
// long atol(const char *nptr);

// double strtod(const char *nptr, char **endptr);
// long strtol(const char *nptr, char **endptr, int base);
// unsigned long strtoul(const char *nptr, char **endptr, int base);

// int rand(void);
// void srand(unsigned int seed);

// void *calloc(size_t nmemb, size_t size);
void *malloc(size_t size);
// void *realloc(void *ptr, size_t size);
void free(void *ptr);

// void abort(void);
// void exit(int status);

// int atexit(void (*func)(void));
// int system(const char *command);

// char *getenv(const char *name);
// void *bsearch(const void *key, const void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));
// void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));

int abs(int x);
long labs(long x);
long long llabs(long long x);

// div_t div(int numer, int denom); ??
// ldiv_t ldiv(long numer, long denom); ??

_End_C_Header;
