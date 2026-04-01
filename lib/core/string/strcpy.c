#include <hubble/string.h>

char *strcpy(char *dest, const char *src)
{
	char *orig = dest;
	while ((*dest++ = *src++))
		;
	return orig;
}
