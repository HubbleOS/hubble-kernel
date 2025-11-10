#pragma once

#include <stdint.h>

typedef struct
{
	uint64_t heap_start;
	uint64_t heap_size;
	uint64_t pml4_phys;
} ram_info_t;
