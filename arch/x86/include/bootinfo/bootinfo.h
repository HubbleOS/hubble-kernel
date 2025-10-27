#pragma once

#include <bootinfo/framebuffer.h>
#include <bootinfo/raminfo.h>
#include <bootinfo/diskinfo.h>

#include <stdint.h>

typedef struct
{
	framebuffer_info_t *framebuffer;
	ram_info_t *memory_map;
	ramdisk_info_t *disk_info;
} BootInfo;

extern BootInfo *g_boot_info;
