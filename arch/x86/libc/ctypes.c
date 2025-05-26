#include "ctype.h"

int isalnum(int c) { return isalpha(c) || isdigit(c); }

int isalpha(int c) { return isupper(c) || islower(c); }

// int iscntrl(int c);

int isdigit(int c) { return c >= '0' && c <= '9'; }

// int isgraph(int c);

int islower(int c) { return c >= 'a' && c <= 'z'; }

// int isprint(int c);

// int ispunct(int c);

int isspace(int c)
{
	return c == ' ' || c == '\t' || c == '\n' ||
		   c == '\v' || c == '\f' || c == '\r';
}

int isupper(int c) { return c >= 'A' && c <= 'Z'; }

// int isxdigit(int c);

int tolower(int c)
{
	if (isupper(c))
		return c + ('a' - 'A');
	return c;
}

int toupper(int c)
{
	if (islower(c))
		return c - ('a' - 'A');
	return c;
}
