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
  uint16_t fb_bpp;
  /* Bit position and width of each colour channel in a pixel. */
  uint8_t fb_red_shift, fb_red_size;
  uint8_t fb_green_shift, fb_green_size;
  uint8_t fb_blue_shift, fb_blue_size;
} platform_info_t;

extern platform_info_t g_platform;
