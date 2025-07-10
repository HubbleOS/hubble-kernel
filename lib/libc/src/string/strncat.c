#include <string.h>

char *strncat(char *dest, const char *src, size_t n)
{
	char *a = dest;

	dest += strlen(dest);
	while (n-- && *src)
		*dest++ = *src++;

	*dest = '\0';

	return a;
}
