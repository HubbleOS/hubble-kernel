/**
 * @file mouse.h
 * @brief Mouse state and input handling
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Mouse state structure.
 *
 * Contains position and button states.
 */
typedef struct {
  int32_t x, y;       /**< Cursor position */
  bool left;          /**< Left button state */
  bool right;         /**< Right button state */
  bool middle;        /**< Middle button state */
  bool left_clicked;  /**< Left click event */
  bool right_clicked; /**< Right click event */
} mouse_t;

// Global mouse state
extern mouse_t *mouse;

/**
 * @brief Get global mouse state.
 *
 * @return Pointer to mouse structure
 */
mouse_t *get_mouse_info(void);
