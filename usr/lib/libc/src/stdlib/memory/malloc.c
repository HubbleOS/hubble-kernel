#include <stdlib.h>
#include "mem.h"

#include <sys/syscall.h>

void *mmap(uint64_t addr, size_t length, int prot, int flags,
	   int fd, uint64_t offset)
{
	return (uint64_t *)syscall6(3, addr, length, prot, flags, fd, offset);
}

void *malloc(size_t size)
{
	if (size == 0)
		return NULL;

	heap_block_t *block = heap_start;

	// searching for a suitable free block
	while (block)
	{
		if (block->free && block->size >= size)
		{
			block->free = false;
			return (void *)(block + 1);
		}
		block = block->next;
	}

	// there is no suitable block - select via mmap
	size_t total_size = HEAP_BLOCK_SIZE + size;
	void *mem = mmap(
	    0,		// address = 0 -> kernel/VM itself will find a free area
	    total_size, // size of allocated memory
	    0x3,	// PROT_READ | PROT_WRITE
	    0x22,	// MAP_ANONYMOUS | MAP_PRIVATE
	    -1,		// fd = -1 for anonymous mmap
	    0		// offset = 0 for anonymous mmap
	);
	if (!mem)
		return NULL;

	block = (heap_block_t *)mem;
	block->size = size;
	block->free = false;
	block->next = NULL;

	// цепляем в список
	if (!heap_start)
	{
		heap_start = block;
		heap_end = block;
	}
	else
	{
		heap_end->next = block;
		heap_end = block;
	}

	return (void *)(block + 1);
}
