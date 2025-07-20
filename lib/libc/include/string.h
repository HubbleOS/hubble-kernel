#pragma once

#include <_cheader.h>
#include <stddef.h>
// ###########################################################################
#include <stdint.h>
// ###########################################################################

_Begin_C_Header;

// Memory manipulation
void *memset(void *dest, int c, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
void *memchr(const void *s, int c, size_t n);

// String manipulation
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);
char *strcat(char *dest, const char *src);
char *strncat(char *dest, const char *src, size_t n);
// ###########################################################################
size_t strxfrm(char *dest, const char *src, size_t n);
// ###########################################################################

// String examination
size_t strlen(const char *s);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
// ###########################################################################
int strcoll(const char *s1, const char *s2);
// ###########################################################################
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
// size_t strspn(const char *s1, const char *s2);
// size_t strcspn(const char *s1, const char *s2);
// char *strpbrk(const char *s1, const char *s2);
// char *strstr(const char *s1, const char *s2);
// char *strtok(char *str, const char *delim);

// Miscellaneous
// char *strerror(int errnum);

// ###########################################################################
size_t strnlen(const char *s, size_t maxlen);
// ###########################################################################

_End_C_Header;
