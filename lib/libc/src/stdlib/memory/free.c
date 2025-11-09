#include <stdlib.h>

#include <mm/mm.h>
#include <mm/pmm.h>

// static memory_ops_t *memory_ops = &heap_memory_ops;

// void free(void *ptr) { memory_ops->free(ptr); }
void free(void *ptr) {}
