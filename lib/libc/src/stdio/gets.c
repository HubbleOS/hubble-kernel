#include <stdio.h>

char *gets(char *s) { return fgets(s, 256, stdin); }
