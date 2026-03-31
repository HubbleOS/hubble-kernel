#include "ctype.h"

// Returns non-zero if c is alphanumeric (letter or digit).
int isalnum(int c) { return isalpha(c) || isdigit(c); }

// Returns non-zero if c is alphabetic letter.
int isalpha(int c) { return ((unsigned)c | 32) - 'a' < 26; }

// Returns non-zero if c is a blank character (' ' or '\t').
int isblank(int c) { return c == ' ' || c == '\t'; }

// Returns non-zero if c is a control character.
int iscntrl(int c) { return (unsigned)c < 0x20 || c == 0x7f; }

// Returns non-zero if c is a decimal digit ('0'..'9').
int isdigit(int c) { return (unsigned)c - '0' < 10; }

// Returns non-zero if c has a graphical representation excluding space.
int isgraph(int c) { return (unsigned)c - 0x21 < 0x5e; }

// Returns non-zero if c is a lowercase letter.
int islower(int c) { return (unsigned)c - 'a' < 26; }

// Returns non-zero if c is printable including space.
int isprint(int c) { return (unsigned)c - 0x20 < 0x5f; }

// Returns non-zero if c is a punctuation character.
int ispunct(int c) { return isgraph(c) && !isalnum(c); }

// Returns non-zero if c is a whitespace character (space, tab, newline, etc.).
int isspace(int c) { return c == ' ' || (unsigned)c - '\t' < 5; }

// Returns non-zero if c is an uppercase letter.
int isupper(int c) { return (unsigned)c - 'A' < 26; }

// Returns non-zero if c is a hexadecimal digit (0-9, a-f, A-F).
int isxdigit(int c) { return isdigit(c) || ((unsigned)c | 32) - 'a' < 6; }

// Converts c to lowercase if it is uppercase.
int tolower(int c) { return isupper(c) ? c | 0x20 : c; }

// Converts c to uppercase if it is lowercase.
int toupper(int c) { return islower(c) ? c & 0x5f : c; }
