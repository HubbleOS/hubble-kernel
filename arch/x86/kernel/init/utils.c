#include "utils.h"

#include <bootinfo/bootinfo.h>
#include "higher_half.h"

// BSS section markers from linker
extern uint8_t _bss_start;
extern uint8_t _bss_end;
extern symbol _kernel_start, _kernel_end;

// Helpers

inline void clear_bss(void)
{
	for (char *ptr = &_bss_start; ptr < &_bss_end; ptr++)
		*ptr = 0;
}
