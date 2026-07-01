/**
 * @file free.c
 * @brief Free allocated memory
 */

#include "mem.h"
#include <stdlib.h>

void free(void *ptr) {
  if (!ptr)
    return;

  heap_block_t *block = (heap_block_t *)ptr - 1;
  block->free = true;
}
