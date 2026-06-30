/**
 * @file memchr.c
 * @brief Memory scanning: find first occurrence of a byte
 */

#include <hubble/string.h>

/**
 * @brief Scan memory for a byte value
 * @param src Memory region to scan
 * @param c Byte value to search for
 * @param n Number of bytes to scan
 * @return Pointer to the first occurrence of c, or NULL if not found
 */
void *memchr(const void *src, int c, size_t n)
{
	const unsigned char *s = src;
	c = (unsigned char)c;

	while (n && *s != c)
		s++, n--;

	return n ? (void *)s : 0;
}
