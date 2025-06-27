#include "stdio.h"
#include "ctype.h"

static void skip_whitespace()
{
	int c;
	do
	{
		c = getchar();
	} while (isspace(c));
	ungetc(c, stdin);
}

int vscanf(const char *format, va_list args)
{
	int assigned = 0;
	while (*format)
	{
		if (*format == '%')
		{
			format++;
			switch (*format)
			{
			case 'd':
			{
				int *ptr = va_arg(args, int *);
				int num = 0;
				int sign = 1;

				skip_whitespace();

				int c = getchar();
				if (c == '-')
				{
					sign = -1;
					c = getchar();
				}

				if (!isdigit(c))
					return assigned;

				do
				{
					num = num * 10 + (c - '0');
					c = getchar();
				} while (isdigit(c));

				*ptr = num * sign;
				assigned++;
				break;
			}
			case 's':
			{
				char *str = va_arg(args, char *);
				skip_whitespace();
				int c;
				while ((c = getchar()) != EOF && !isspace(c))
					*str++ = (char)c;
				*str = '\0';
				assigned++;
				break;
			}
			case 'c':
			{
				char *ch = va_arg(args, char *);
				int c = getchar();
				if (c == EOF)
					return assigned;
				*ch = (char)c;
				assigned++;
				break;
			}
			default:
				break;
			}
		}
		else
		{
			format++;
		}
	}

	return assigned;
}
