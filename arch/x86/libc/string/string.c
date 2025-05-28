#include "string.h"

char *strcpy(char *dest, const char *src)
{
	char *orig = dest;
	while ((*dest++ = *src++))
		;
	return orig;
}

char *strncpy(char *dest, const char *src, size_t n)
{
	size_t i = 0;
	for (; i < n && src[i]; i++)
	{
		dest[i] = src[i];
	}
	for (; i < n; i++)
	{
		dest[i] = '\0';
	}
	return dest;
}

// char *strcat(char *dest, const char *src);
// char *strncat(char *dest, const char *src, size_t n);

int strcmp(const char *s1, const char *s2)
{
	while (*s1 && (*s1 == *s2))
	{
		s1++;
		s2++;
	}
	return *(unsigned char *)s1 - *(unsigned char *)s2;
}

int strncmp(const char *s1, const char *s2, size_t n)
{
	while (n && *s1 && (*s1 == *s2))
	{
		s1++;
		s2++;
		n--;
	}
	if (n == 0)
		return 0;
	return *(unsigned char *)s1 - *(unsigned char *)s2;
}

char *strchr(const char *s, int c)
{
	while (*s)
	{
		if (*s == (char)c)
			return (char *)s;
		s++;
	}
	return c == '\0' ? (char *)s : NULL;
}

// char *strrchr(const char *s, int c);

// size_t strspn(const char *s1, const char *s2);
// size_t strcspn(const char *s1, const char *s2);
// char *strpbrk(const char *s1, const char *s2);
// char *strstr(const char *s1, const char *s2);

size_t strlen(const char *s)
{
	size_t len = 0;
	while (s[len])
		len++;
	return len;
}

// char *strerror(int errnum);

// char *strtok(char *str, const char *delim);

void *memcpy(void *dest, const void *src, size_t n)
{
	unsigned char *d = dest;
	const unsigned char *s = src;
	while (n--)
		*d++ = *s++;
	return dest;
}

// void *memmove(void *dest, const void *src, size_t n);

int memcmp(const void *s1, const void *s2, size_t n)
{
	const unsigned char *p1 = s1;
	const unsigned char *p2 = s2;
	while (n--)
	{
		if (*p1 != *p2)
			return *p1 - *p2;
		p1++;
		p2++;
	}
	return 0;
}

// void *memchr(const void *s, int c, size_t n);

void *memset(void *s, int c, size_t n)
{
	unsigned char *p = s;
	while (n--)
		*p++ = (unsigned char)c;
	return s;
}
