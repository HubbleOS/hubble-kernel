/**
 * @file strcpy.c
 * @brief String copy
 */

#include <hubble/string.h>

/**
 * @brief Copy a string
 * @param dest Destination buffer
 * @param src Source string
 * @return Pointer to dest
 */
char *strcpy(char *dest, const char *src)
{
	char *orig = dest;
	while ((*dest++ = *src++))
		;
	return orig;
}
