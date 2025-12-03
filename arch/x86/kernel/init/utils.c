#include "utils.h"

#include <bootinfo/bootinfo.h>
#include "higher_half.h"

// BSS section markers from linker
extern symbol _bss_start, _bss_end;
extern symbol _kernel_start, _kernel_end;

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

inline void clear_bss(void)
{
	for (char *ptr = &_bss_start; ptr < &_bss_end; ptr++)
		*ptr = 0;
}

void relocate_boot_info(BootInfo *bi)
{
	if (bi->framebuffer && IS_PHYSICAL(bi->framebuffer))
		bi->framebuffer = PHYS_TO_VIRT(bi->framebuffer);

	if (bi->memory_map && IS_PHYSICAL(bi->memory_map))
		bi->memory_map = PHYS_TO_VIRT(bi->memory_map);
}
