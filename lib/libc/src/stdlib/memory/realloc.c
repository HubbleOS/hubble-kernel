#include <stdlib.h>

#include <mm/mm.h>
#include <mm/pmm.h>

// static memory_ops_t *memory_ops = &heap_memory_ops;

// void *realloc(void *ptr, size_t size) { return memory_ops->realloc(ptr, size); }
void *realloc(void *ptr, size_t size) {}
