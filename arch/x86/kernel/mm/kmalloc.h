#pragma once

#include <stdint.h>
#include <stddef.h>

// Initialization heap
void kmalloc_init(void);

// Basic functions
void *kmalloc(size_t size);
void *kzalloc(size_t size); // Zeroed
void kfree(void *ptr);
void *krealloc(void *ptr, size_t new_size);

// Aligned allocation
void *kmalloc_aligned(size_t size, size_t alignment);

// Statistics
void kmalloc_stats(void);
