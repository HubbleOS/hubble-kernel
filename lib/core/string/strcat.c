/**
 * @file strcat.c
 * @brief String concatenation
 */

#include <hubble/string.h>

/**
 * @brief Append a string to the end of another
 * @param dest Destination string
 * @param src Source string to append
 * @return Pointer to dest
 */
char *strcat(char *dest, const char *src)
{
	strcpy(dest + strlen(dest), src);
	return dest;
}
