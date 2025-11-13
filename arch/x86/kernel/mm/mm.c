#include "mm.h"
#include "kmalloc.h"

memory_ops_t heap_memory_ops = {
    .malloc = kmalloc,
    .realloc = krealloc,
    .free = kfree,
};
