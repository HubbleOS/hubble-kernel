#include <hubble/string.h>

// Temporary implementation of strcoll()
// TODO: Implement locale-aware string comparison in the future.
// For now, we fall back to simple binary comparison using strcmp.

int strcoll(const char *s1, const char *s2)
{
	return strcmp(s1, s2);
}
