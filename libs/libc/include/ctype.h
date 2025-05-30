#ifndef _CTYPE_H_
#define _CTYPE_H_

#ifdef __cplusplus
extern "C"
{
#endif

	// Returns non-zero if c is alphanumeric (letter or digit).
	int isalnum(int);

	// Returns non-zero if c is alphabetic letter.
	int isalpha(int);

	// Returns non-zero if c is a blank character (' ' or '\t').
	int isblank(int);

	// Returns non-zero if c is a control character.
	int iscntrl(int);

	// Returns non-zero if c is a decimal digit ('0'..'9').
	int isdigit(int);

	// Returns non-zero if c has a graphical representation excluding space.
	int isgraph(int);

	// Returns non-zero if c is a lowercase letter.
	int islower(int);

	// Returns non-zero if c is printable including space.
	int isprint(int);

	// Returns non-zero if c is a punctuation character.
	int ispunct(int);

	// Returns non-zero if c is a whitespace character (space, tab, newline, etc.).
	int isspace(int);

	// Returns non-zero if c is an uppercase letter.
	int isupper(int);

	// Returns non-zero if c is a hexadecimal digit (0-9, a-f, A-F).
	int isxdigit(int);

	// Converts c to lowercase if it is uppercase.
	int tolower(int);

	// Converts c to uppercase if it is lowercase.
	int toupper(int);

#ifdef __cplusplus
}
#endif

#endif