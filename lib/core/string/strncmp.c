/**
 * @file strncmp.c
 * @brief Bounded string comparison
 */

#include <hubble/string.h>

/**
 * @brief Compare up to n characters of two strings
 * @param s1 First string
 * @param s2 Second string
 * @param n Maximum number of characters to compare
 * @return 0 if equal, negative if s1 < s2, positive if s1 > s2
 */
int strncmp(const char *s1, const char *s2, size_t n)
{
	while (n && *s1 && (*s1 == *s2))
		s1++, s2++, n--;

	if (n == 0)
		return 0;

	return *(unsigned char *)s1 - *(unsigned char *)s2;
}
