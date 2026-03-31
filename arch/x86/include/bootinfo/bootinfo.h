#pragma once

#include <bootinfo/framebuffer.h>
#include <bootinfo/raminfo.h>
#include <bootinfo/rsdp.h>

#include <stdint.h>

typedef struct
{
	framebuffer_info_t framebuffer;
	ram_info_t memory_map;
	void *rsdp;

} BootInfo;
extern BootInfo *g_boot_info;
void clear_bss(void);
