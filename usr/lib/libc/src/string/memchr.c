/**
 * @file memchr.c
 * @brief Scan memory for a byte
 */

#include <string.h>

void *memchr(const void *src, int c, size_t n)
{
	const unsigned char *s = src;
	c = (unsigned char)c;

	while (n && *s != c)
		s++, n--;

	return n ? (void *)s : 0;
}
