#include <stdlib.h>

#include <mm/mm.h>
#include <mm/pmm.h>
#include <mm/kmalloc.h>

static memory_ops_t *memory_ops = &heap_memory_ops;

void *realloc(void *ptr, size_t size, kmalloc_flags_t flags) { return memory_ops->realloc(ptr, size, flags); }
