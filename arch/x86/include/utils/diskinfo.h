#pragma once

#include <stdint.h>

typedef struct
{
	void *ramdisk_base;
	uint64_t ramdisk_size;
} __attribute__((packed)) ramdisk_info_t;
