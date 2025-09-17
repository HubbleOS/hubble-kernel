#include <stdlib.h>

#include <mm/heap.h>
#include <mm/pmm.h>

static memory_ops_t *memory_ops = &heap_memory_ops;

void free(void *ptr) { memory_ops->free(ptr); }
