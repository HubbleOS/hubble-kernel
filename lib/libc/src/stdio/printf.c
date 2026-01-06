#include <stdio.h>

// int printf(const char *format, ...)
// {
// 	va_list args;
// 	va_start(args, format);
// 	int ret = vfprintf(stdout, format, args);
// 	va_end(args);
// 	return ret;
// }

int printf(const char *fmt, ...) // temporary simple implementation
{
	va_list ap;
	va_start(ap, fmt);
	for (const char *p = fmt; *p; p++)
	{
		if (*p == '%')
		{
			p++;
			if (*p == 'c')
				putchar(va_arg(ap, int));
			else if (*p == 's')
			{
				char *s = va_arg(ap, char *);
				while (*s)
					putchar(*s++);
			}
			else
				putchar(*p);
		}
		else
			putchar(*p);
	}
	va_end(ap);
	return 0;
}
