#pragma once

#include <bootinfo/framebuffer.h>
#include <bootinfo/raminfo.h>

#include <stdint.h>

typedef struct
{
	framebuffer_info_t framebuffer_data; // Встроенная структура
	ram_info_t memory_data;		     // Встроенная структура
	framebuffer_info_t *framebuffer;     // Указатель для совместимости
	ram_info_t *memory_map;		     // Указатель для совместимости
} BootInfo;
extern BootInfo *g_boot_info;
void clear_bss(void);
void relocate_boot_info(BootInfo *bi);
