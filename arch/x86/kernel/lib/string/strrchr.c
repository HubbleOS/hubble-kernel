#include <string.h>

char *strrchr(const char *s, int c)
{
	const char *end = s + strlen(s);
	unsigned char uc = (unsigned char)c;

	while (end != s)
	{
		end--;
		if (*end == uc)
			return (char *)end;
	}

	return uc == *s ? (char *)s : NULL;
}
