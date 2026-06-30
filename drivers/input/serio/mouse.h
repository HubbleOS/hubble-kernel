/**
 * @file mouse.h
 * @brief PS/2 mouse driver — state tracking and VFS interface
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <mm/vmm.h>

/**
 * @brief Mouse state structure
 */
typedef struct
{
	int32_t x, y;
	bool left, right, middle;
	bool left_clicked;
	bool right_clicked;
} mouse_t;

/**
 * @brief Initialise the PS/2 mouse
 */
void mouse_init(void);

/**
 * @brief Get a pointer to the global mouse state
 * @return Pointer to the mouse_t structure
 */
mouse_t *get_mouse_info(void);

/**
 * @brief MMAP handler — exposes the mouse state to user-space
 */
uint64_t mouse_mmap(uint64_t offset, size_t size);

/**
 * @brief Read handler — copies the current mouse state into a buffer
 */
uint64_t mouse_read_file(uint64_t offset, size_t size, void *buf);
