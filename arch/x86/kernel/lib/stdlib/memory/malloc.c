#include <stdlib.h>

#include <mm/mm.h>
#include <mm/pmm.h>

static memory_ops_t *memory_ops = &heap_memory_ops;

void *malloc(size_t size) { return memory_ops->malloc(size); }
