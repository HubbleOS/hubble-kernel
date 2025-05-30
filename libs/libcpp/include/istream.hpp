#ifndef _IOSTREAM_HPP
#define _IOSTREAM_HPP

namespace std
{

	class istream
	{
	public:
		istream &operator>>(char *str);
		istream &operator>>(char &c);
		istream &operator>>(int &num);
		// ...
	};

	extern istream cin;

} // namespace std

#endif // LIBCPP_ISTREAM_HPP
