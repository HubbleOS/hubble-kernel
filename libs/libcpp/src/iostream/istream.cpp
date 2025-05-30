#include <cstdio>
#include "istream.hpp"

namespace std
{

	istream &istream::operator>>(char *str)
	{
		scanf("%s", str);
		return *this;
	}

	istream &istream::operator>>(char &c)
	{
		c = getchar();
		return *this;
	}

	istream &istream::operator>>(int &num)
	{
		scanf("%d", &num);
		return *this;
	}

	istream cin;

}
