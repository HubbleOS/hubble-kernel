#include "heap.h"

static uint64_t heap_ptr = 0;
static uint64_t heap_end = 0;

void heap_init(uint64_t heap_start, uint64_t heap_size)
{
    heap_ptr = heap_start;
    heap_end = heap_start + heap_size;
    volatile uint8_t *ptr = (volatile uint8_t *)heap_start;
    for (int i = 0; i < 1024; i++)
    {
        ptr[i] = 0xAA;
    }
}

void *kmalloc(size_t size)
{
    if (heap_ptr + size > heap_end)
        return NULL;
    void *ptr = (void *)heap_ptr;
    heap_ptr += size;
    return ptr;
}

void *kmalloc_aligned(size_t size, size_t align)
{
    uint64_t aligned_ptr = (heap_ptr + align - 1) & ~(align - 1);
    if (aligned_ptr + size > heap_end)
        return NULL;
    void *ptr = (void *)aligned_ptr;
    heap_ptr = aligned_ptr + size;
    return ptr;
}
void kfree(void *ptr) {}