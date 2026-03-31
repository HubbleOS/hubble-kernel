#pragma once

#include <stddef.h>
// ###########################################################################
#include <stdint.h>
// ###########################################################################

// Memory manipulation
void *memset(void *, int, size_t);
void *memcpy(void *, const void *, size_t);
void *memmove(void *, const void *, size_t);
int memcmp(const void *, const void *, size_t);
void *memchr(const void *, int, size_t);

// String manipulation
char *strcpy(char *, const char *);
char *strncpy(char *, const char *, size_t);
char *strcat(char *, const char *);
char *strncat(char *, const char *, size_t);
char *strdup(const char *);
// ###########################################################################
size_t strxfrm(char *, const char *, size_t);
// ###########################################################################

// String examination
size_t strlen(const char *);
int strcmp(const char *, const char *);
int strncmp(const char *, const char *, size_t);
// ###########################################################################
int strcoll(const char *, const char *);
// ###########################################################################
char *strchr(const char *, int);
char *strrchr(const char *, int);
// size_t strspn(const char *s1, const char *s2);
// size_t strcspn(const char *s1, const char *s2);
// char *strpbrk(const char *s1, const char *s2);
// char *strstr(const char *s1, const char *s2);
// char *strtok(char *str, const char *delim);

// Miscellaneous
// char *strerror(int errnum);

// ###########################################################################
size_t strnlen(const char *, size_t);
// ###########################################################################
