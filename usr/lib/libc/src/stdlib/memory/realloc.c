#include <stdlib.h>
#include "mem.h"

void *realloc(void *ptr, size_t size)
{
	if (!ptr)
		return malloc(size);

	heap_block_t *block = (heap_block_t *)ptr - 1;
	if (block->size >= size)
		return ptr; // the current block is quite large

	void *new_ptr = malloc(size);
	if (!new_ptr)
		return NULL;

	// copy data
	for (size_t i = 0; i < block->size; i++)
	{
		((char *)new_ptr)[i] = ((char *)ptr)[i];
	}

	free(ptr);
	return new_ptr;
}
