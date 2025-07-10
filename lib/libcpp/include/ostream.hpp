#ifndef _OSTREAM_HPP
#define _OSTREAM_HPP

namespace std
{

	class ostream
	{
	public:
		ostream &operator<<(const char *str);
		ostream &operator<<(char c);
		ostream &operator<<(int num);
		// ...
	};

	extern ostream cout;

} // namespace std

#endif // LIBCPP_OSTREAM_HPP
