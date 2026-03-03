#include <stdlib.h>

#include <mm/mm.h>
#include <mm/pmm.h>
#include <mm/kmalloc.h>

static memory_ops_t *memory_ops = &heap_memory_ops;

void *realloc(void *ptr, size_t size) { return memory_ops->realloc(ptr, size, GFP_ZERO); }
