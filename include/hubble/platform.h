#pragma once

/**
 * @brief Platform information populated from bootloader data.
 */

#include <stddef.h>
#include <stdint.h>

typedef struct {
  uint64_t fb_base;
  uint32_t fb_width;
  uint32_t fb_height;
  uint32_t fb_pitch;
} platform_info_t;

extern platform_info_t g_platform;
