#include "mm.h"
#include "slab.h"

memory_ops_t heap_memory_ops = {
    .malloc = kmalloc,
    .realloc = kmalloc,
    .free = kfree,
};
