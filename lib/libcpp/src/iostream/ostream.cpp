#include <cstdio>
#include "iostream.hpp"

namespace std
{
	ostream &ostream::operator<<(const char *str)
	{
		printf("%s", str);
		return *this;
	}

	ostream &ostream::operator<<(char c)
	{
		putchar(c);
		return *this;
	}

	ostream &ostream::operator<<(int num)
	{
		printf("%d", num);
		return *this;
	}

	ostream cout;
}
