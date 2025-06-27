#include "stdio.h"

char *fgets(char *s, int size, FILE *stream)
{
	int c;
	int i = 0;
	while ((c = getc(stream)) != EOF)
	{
		if (c == '\n')
			break;
		if (i < size - 1)
			s[i++] = c;
	}
	s[i] = '\0';
	return s;
}
