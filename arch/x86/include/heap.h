#pragma once
#include <stdint.h>
#include <stddef.h>

void heap_init(uint64_t heap_start, uint64_t heap_size);
void *kmalloc(size_t size);
void kfree(void *ptr);