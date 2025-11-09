#pragma once

#include <stdint.h>

typedef struct
{
	uint64_t bootstrap_start;
	uint64_t bootstrap_size;

	uint64_t heap_start;
	uint64_t heap_size;
} ram_info_t;
