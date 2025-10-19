#pragma once

#include <stdint.h>
#include <stddef.h>

void heap_init(uint64_t heap_start, uint64_t heap_size);
void *kmalloc1(size_t size);
void kfree1(void *ptr);

typedef struct
{
	void *(*malloc)(size_t size);
	void (*free)(void *ptr);
} memory_ops_t;

extern memory_ops_t heap_memory_ops;
