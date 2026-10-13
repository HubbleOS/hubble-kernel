/**
 * @file fb.h
 * @brief Framebuffer description libgui draws into (GUI_DEMO platform API).
 *
 * On HubbleOS, base is /dev/fb0 mapped with mmap; see main.c.
 */
#pragma once

#include <stdint.h>

typedef struct {
  uint32_t *base;  /**< Mapped framebuffer memory */
  uint32_t width;  /**< Width in pixels */
  uint32_t height; /**< Height in pixels */
  uint32_t pitch;  /**< Bytes per row */
  uint8_t bpp;     /**< Bits per pixel (libgui needs 32) */
} framebuffer_info_t;
