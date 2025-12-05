#pragma once

#include <stdint.h>
#include <stddef.h>

#include "kmalloc.h"

typedef struct
{
	void *(*malloc)(size_t size);
	void *(*realloc)(void *ptr, size_t size, kmalloc_flags_t flags);
	void (*free)(void *ptr);
} memory_ops_t;

extern memory_ops_t heap_memory_ops;
