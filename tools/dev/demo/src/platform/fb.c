#include "fb.h"
#include <stdlib.h>

/**
 * Allocates framebuffer structure and pixel buffer.
 * Pitch is calculated as width * bytes_per_pixel.
 */
framebuffer_info_t *fb_create(uint32_t width, uint32_t height, uint8_t bpp) {
  framebuffer_info_t *fb = malloc(sizeof(framebuffer_info_t));
  fb->width = width;
  fb->height = height;
  fb->bpp = bpp;
  fb->pitch = width * (bpp / 8);
  fb->base = malloc(fb->pitch * height);
  return fb;
}

/**
 * Frees framebuffer structure and pixel buffer.
 */
void fb_destroy(framebuffer_info_t *fb) {
  free(fb->base);
  free(fb);
}
