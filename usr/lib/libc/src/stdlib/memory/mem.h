#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct heap_block
{
	size_t size;
	bool free;
	struct heap_block *next;
} heap_block_t;

#define HEAP_BLOCK_SIZE sizeof(heap_block_t)
#define HEAP_INCREMENT 0x1000 // 4 KB

static heap_block_t *heap_start = NULL;
static heap_block_t *heap_end = NULL;
