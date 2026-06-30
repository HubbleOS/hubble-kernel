/**
 * @file strncpy.c
 * @brief Bounded string copy
 */

#include <hubble/string.h>

/**
 * @brief Copy up to n characters from a string, padding with nulls
 * @param dest Destination buffer
 * @param src Source string
 * @param n Maximum number of characters to copy
 * @return Pointer to dest
 */
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
