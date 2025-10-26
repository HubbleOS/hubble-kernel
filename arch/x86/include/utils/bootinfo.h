#pragma once

#include <utils/framebuffer.h>
#include <utils/power.h>
#include <utils/raminfo.h>
#include <utils/diskinfo.h>

#include <stdint.h>

typedef struct
{
	framebuffer_info_t *framebuffer;
	ram_info_t *memory_map;
	ramdisk_info_t *disk_info;
	PowerOps power;
} BootInfo;

extern BootInfo *g_boot_info;
