/**
 * @file malloc.c
 * @brief Allocate memory
 */

#include "mem.h"
#include <stdlib.h>
#include <sys/syscall.h>

void *mmap(uint64_t addr, size_t length, int prot, int flags, int fd,
           uint64_t offset) {
  return (uint64_t *)syscall6(3, addr, length, prot, flags, fd, offset);
}

void *malloc(size_t size) {
  if (size == 0)
    return NULL;

  heap_block_t *block = heap_start;

  while (block) {
    if (block->free && block->size >= size) {
      block->free = false;
      return (void *)(block + 1);
    }
    block = block->next;
  }

  size_t total_size = HEAP_BLOCK_SIZE + size;
  void *mem = mmap(0, total_size, 0x3, 0x22, -1, 0);
  if (!mem)
    return NULL;

  block = (heap_block_t *)mem;
  block->size = size;
  block->free = false;
  block->next = NULL;

  if (!heap_start) {
    heap_start = block;
    heap_end = block;
  } else {
    heap_end->next = block;
    heap_end = block;
  }

  return (void *)(block + 1);
}
