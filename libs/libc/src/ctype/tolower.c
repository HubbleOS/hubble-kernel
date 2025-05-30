#include <ctype.h>

int tolower(int c) { return isupper(c) ? c | 0x20 : c; }
