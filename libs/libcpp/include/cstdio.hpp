#ifndef _CSTDIO_H_
#define _CSTDIO_H_

extern "C"
{
#include <stdio.h>
}

namespace std
{
	// Standard input/output functions
	using ::fgets;
	using ::fputs;
	using ::printf;

	//
	using ::putchar;
	using ::puts;
	using ::scanf;

}

#endif