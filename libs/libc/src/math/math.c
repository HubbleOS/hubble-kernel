#include <math.h>

// #define M_PI 3.14159265358979323846264338327

// double sin(double x);
// double cos(double x);
// double tan(double x);

// double asin(double x);
// double acos(double x);
// double atan(double x);
// double atan2(double y, double x);

// double sinh(double x);
// double cosh(double x);
// double tanh(double x);

// double exp(double x);
// double log(double x);
// double log10(double x);

double pow(double x, double n) { return n == 0 ? 1 : x * pow(x, n - 1); }

double sqrt(double x)
{
	double y;
	int p, square, c;

	/* find the surrounding perfect squares */
	p = 0;
	do
	{
		p++;
		square = (p + 1) * (p + 1);
	} while (x > square);

	/* process the root */
	y = (double)p;
	c = 0;
	while (c < 10)
	{
		/* divide and average */
		y = (x / y + y) / 2;
		/* test for success */
		if (y * y == x)
			return (y);
		c++;
	}
	return (y);
}

// double sqrt(double x);

// double ceil(double x);
// double floor(double x);

// double fabs(double x);
// double ldexp(double x, int n);
// double frexp(double x, int *n);

// double modf(double x, double *iptr);
// double fmod(double x, double y);