/**
 * @file fb.h
 * @brief Framebuffer abstraction used by GUI subsystem.
 */
#pragma once

#include <stdint.h>

/**
 * @brief Framebuffer information structure.
 *
 * Describes a linear framebuffer used by the GUI system.
 */
typedef struct {
  uint32_t *base;  /**< Pointer to framebuffer memory */
  uint32_t width;  /**< Framebuffer width in pixels */
  uint32_t height; /**< Framebuffer height in pixels */
  uint32_t pitch;  /**< Bytes per row */
  uint8_t bpp;     /**< Bits per pixel */
} framebuffer_info_t;

/**
 * @brief Create a framebuffer instance.
 *
 * Allocates memory for framebuffer structure and pixel buffer.
 *
 * @param width Framebuffer width in pixels
 * @param height Framebuffer height in pixels
 * @param bpp Bits per pixel
 *
 * @return Pointer to allocated framebuffer_info_t
 */
framebuffer_info_t *fb_create(uint32_t width, uint32_t height, uint8_t bpp);

/**
 * @brief Destroy framebuffer and free allocated memory.
 *
 * @param fb Framebuffer instance
 */
void fb_destroy(framebuffer_info_t *fb);
