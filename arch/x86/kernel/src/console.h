#pragma once

#include <hubble/fb.h>
#include <stdbool.h>

void printk_init(framebuffer_info_t *fb);

/** Grow the console to the whole screen; call once pages can be allocated. */
void console_init_late(void);

/**
 * Give the display to the console (true) or take it away (false). While
 * off, the console keeps drawing into its own buffer but never touches
 * the framebuffer; turning it back on repaints the whole console.
 */
void console_set_output(bool enabled);
bool console_output_enabled(void);

/** Place the console's top-left corner at (x, y) on the framebuffer. */
void console_set_origin(int x, int y);
