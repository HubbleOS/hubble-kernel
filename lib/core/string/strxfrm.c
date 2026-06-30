/**
 * @file strxfrm.c
 * @brief String transformation for locale-aware comparison (stub)
 *
 * Currently performs a simple byte-wise copy suitable for strcmp-based sorting.
 * TODO: Implement locale-aware transformation in the future.
 */

#include <hubble/string.h>

/**
 * @brief Transform a string for locale-aware comparison
 * @param dest Destination buffer
 * @param src Source string
 * @param n Size of destination buffer
 * @return Length of the transformed string (excluding null terminator)
 */
size_t strxfrm(char *dest, const char *src, size_t n)
{
	size_t len = strlen(src);

	if (n != 0)
	{
		size_t copy_len = (len < n - 1) ? len : n - 1;
		memcpy(dest, src, copy_len);
		dest[copy_len] = '\0';
	}

	return len;
}
