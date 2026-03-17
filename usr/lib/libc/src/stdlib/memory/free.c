#include <stdlib.h>
#include "mem.h"

void free(void *ptr)
{
	if (!ptr)
		return;

	heap_block_t *block = (heap_block_t *)ptr - 1;
	block->free = true;

	// the simplest option: do not return memory to the kernel
	// munmap
}
