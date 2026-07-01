/**
 * @file mem.h
 * @brief Internal heap block structure for malloc/free/realloc
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct heap_block {
  size_t size;
  bool free;
  struct heap_block *next;
} heap_block_t;

#define HEAP_BLOCK_SIZE sizeof(heap_block_t)
#define HEAP_INCREMENT 0x1000

static heap_block_t *heap_start = NULL;
static heap_block_t *heap_end = NULL;
