#pragma once

/**
 * @brief Framebuffer information structure.
 */

#include <stdint.h>

typedef struct {
	void     *base;
	uint32_t  width;
	uint32_t  height;
	uint32_t  pitch;
	uint32_t  bpp;
} framebuffer_info_t;
