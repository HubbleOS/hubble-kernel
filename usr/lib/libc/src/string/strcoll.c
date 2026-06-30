/**
 * @file strcoll.c
 * @brief Locale-aware string comparison (fallback to strcmp)
 */

#include <string.h>

int strcoll(const char *s1, const char *s2)
{
	return strcmp(s1, s2);
}
