/**
 * @file memcpy.c
 * @brief Memory copy (non-overlapping)
 */

#include <hubble/string.h>

/**
 * @brief Copy memory region (non-overlapping)
 * @param dest Destination buffer
 * @param src Source buffer
 * @param n Number of bytes to copy
 * @return Pointer to dest
 */
void *memcpy(void *dest, const void *src, size_t n)
{
	unsigned char *d = dest;
	const unsigned char *s = src;
	while (n--)
		*d++ = *s++;
	return dest;
}
