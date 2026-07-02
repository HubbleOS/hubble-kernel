/**
 * @file strrchr.c
 * @brief String scanning: find last occurrence of a character
 */

#include <hubble/string.h>

/**
 * @brief Find the last occurrence of a character in a string
 * @param s String to search
 * @param c Character to find
 * @return Pointer to the last occurrence of c, or NULL if not found
 */
char *strrchr(const char *s, int c) {
  const char *end = s + strlen(s);
  unsigned char uc = (unsigned char)c;

  while (end != s) {
    end--;
    if (*end == uc)
      return (char *)end;
  }

  return uc == *s ? (char *)s : NULL;
}
