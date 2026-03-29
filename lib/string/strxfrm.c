#include <hubble/string.h>

// Temporary implementation of strxfrm()
// TODO: Implement locale-aware transformation in the future.
// For now, we fall back to a simple byte-wise copy suitable for strcmp-based sorting.

size_t strxfrm(char *dest, const char *src, size_t n)
{
	size_t len = strlen(src);

	if (n != 0)
	{
		// Copy at most n-1 characters and null-terminate
		size_t copy_len = (len < n - 1) ? len : n - 1;
		memcpy(dest, src, copy_len);
		dest[copy_len] = '\0';
	}

	return len;
}
