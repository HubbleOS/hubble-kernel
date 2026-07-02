#pragma once

/**
 * @brief Character classification and conversion (subset of <ctype.h>).
 */

#include <_cheader.h>

_Begin_C_Header;

int isalnum(int c);
int isalpha(int c);
int isblank(int c);
int iscntrl(int c);
int isdigit(int c);
int isgraph(int c);
int islower(int c);
int isprint(int c);
int ispunct(int c);
int isspace(int c);
int isupper(int c);
int isxdigit(int c);

int tolower(int c);
int toupper(int c);

_End_C_Header;
