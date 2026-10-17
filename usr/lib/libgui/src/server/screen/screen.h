#pragma once
#include <stddef.h>
#include <stdint.h>
#include <utils/color/color.h>
/* The screen libgui draws into: on HubbleOS, /dev/fb0 mapped with mmap
 * (apps/desktop). libgui needs 32 bpp XRGB. */
typedef struct {
  uint32_t *base;  /**< Mapped framebuffer memory */
  uint32_t width;  /**< Width in pixels */
  uint32_t height; /**< Height in pixels */
  uint32_t pitch;  /**< Bytes per row */
  uint8_t bpp;     /**< Bits per pixel */
} framebuffer_info_t;
extern framebuffer_info_t *g_fb;
extern uint32_t *framebuffer_back;
extern int fb_width;
extern int fb_height;

void screen_init(framebuffer_info_t *fb);
void screen_present_rect(int x, int y, int w, int h);
