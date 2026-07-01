/**
 * @file bootinfo.h
 * @brief Boot information structure passed from bootloader to kernel
 *
 * Aggregates framebuffer, memory map, and RSDP information
 * provided by the bootloader at startup.
 */

#pragma once

#include <bootinfo/raminfo.h>
#include <bootinfo/rsdp.h>
#include <hubble/fb.h>

#include <stdint.h>

/**
 * @brief Top-level boot information structure
 */
typedef struct {
  framebuffer_info_t framebuffer;
  ram_info_t memory_map;
  void *rsdp;
} BootInfo;

extern BootInfo *g_boot_info;
