/**
 * @file console.c
 * @brief Early serial + framebuffer console
 *
 * Provides a simple putchar implementation that writes to the
 * serial port (COM1) and, if a framebuffer is available, renders
 * 8x8 characters with scrolling.
 */

#include "textout.h"
#include <hubble/color.h>
#include <hubble/fb.h>
#include <hubble/printk.h>
#include <hubble/string.h>
#include <io.h>
#include <stdbool.h>

/* -- Constants ------------------------------------------------- */

#define CHAR_WIDTH 8
#define CHAR_HEIGHT 8

/* -- Console State --------------------------------------------- */

static int early_x = 0;
static int early_y = 0;
static framebuffer_info_t *early_fb = NULL;
static color_t serial_color = COLOR_WHITE;

/* -- Shadow framebuffer (fast scrolling) ------------------------ */

/* Scrolling has to know what's already on screen to shift it up a row.
 * Reading that back from the real framebuffer (as this used to) is fine
 * under TCG, which doesn't model real GPU memory timing, but on real
 * hardware and under KVM the framebuffer is typically write-combined:
 * reads from it are drastically slower than writes, and a full-screen
 * scroll reads every single row. That's the whole lag - per-character
 * drawing stays fast because it only touches an 8x8 patch either way.
 *
 * Fix: keep a plain-RAM mirror of what's on screen, updated with the
 * same small, already-fast per-character write draw_char() does (not a
 * bulk read of the framebuffer), and scroll THAT (ordinary RAM, no MMIO
 * penalty) - only the final result is written to the real framebuffer,
 * once, sequentially, which is the cheap direction for write-combined
 * memory.
 *
 * This runs before the memory allocator is up (printk_init() is called
 * before boot_memory_init() in main.c - see init/main.c), so it has to
 * be a static buffer sized for a generous but bounded resolution rather
 * than allocated to fit whatever framebuffer we're actually handed.
 * A framebuffer bigger than this falls back to the old, slow-but-correct
 * direct path. */
#define FB_SHADOW_MAX_BYTES (8u * 1024 * 1024)
static uint8_t g_fb_shadow[FB_SHADOW_MAX_BYTES];
static bool g_fb_shadow_active = false;

/* -- Serial Output --------------------------------------------- */

/**
 * @brief Write a single character to serial port COM1
 *
 * @param c Character to write
 */
void serial_putc(char c) { outb(0x3f8, c); }

/**
 * @brief Write a null-terminated string to the serial port
 *
 * @param str String to write
 */
static void serial_write(const char *str) {
  while (*str)
    serial_putc(*str++);
}

/**
 * @brief Return the ANSI escape sequence for a given colour
 *
 * @param color Kernel colour constant
 * @return ANSI escape string
 */
static const char *serial_ansi_color(color_t color) {
  switch (color) {
  case COLOR_BLACK:
    return "\033[30m";
  case COLOR_RED:
    return "\033[91m";
  case COLOR_GREEN:
    return "\033[92m";
  case COLOR_YELLOW:
    return "\033[93m";
  case COLOR_BLUE:
    return "\033[94m";
  case COLOR_WHITE:
    return "\033[97m";
  default:
    return "\033[0m";
  }
}

/**
 * @brief Set the serial ANSI colour (no-op if already set)
 *
 * @param color Desired colour
 */
static void serial_set_color(color_t color) {
  if (serial_color == color)
    return;

  serial_write(serial_ansi_color(color));
  serial_color = color;
}

/* -- Framebuffer Output ---------------------------------------- */

/**
 * @brief Write one character to both serial and framebuffer
 *
 * Handles newline, carriage return, backspace, and tab.
 * Scrolls the framebuffer when the cursor reaches the bottom.
 *
 * @param c     Character to write
 * @param color Colour for the character
 */
void early_putchar_color(char c, color_t color) {
  serial_set_color(color);
  serial_putc(c);

  if (!early_fb || !early_fb->base)
    return;

  if (c == '\n') {
    early_x = 0;
    early_y += CHAR_HEIGHT;

    if (early_y + CHAR_HEIGHT > (int)early_fb->height) {
      size_t line_size = early_fb->pitch;
      uint8_t *fb_base = (uint8_t *)early_fb->base;

      if (g_fb_shadow_active) {
        /* Shift rows within the RAM shadow (plain memory, no MMIO read
         * penalty), then flush the whole thing to the real framebuffer
         * in one sequential write pass - the cheap direction for
         * write-combined GPU memory. */
        for (int y = 0; y < (int)early_fb->height - CHAR_HEIGHT; y++)
          memcpy(g_fb_shadow + (size_t)y * line_size,
                 g_fb_shadow + (size_t)(y + CHAR_HEIGHT) * line_size,
                 line_size);

        for (int y = (int)early_fb->height - CHAR_HEIGHT;
             y < (int)early_fb->height; y++)
          memset(g_fb_shadow + (size_t)y * line_size, 0, line_size);

        memcpy(fb_base, g_fb_shadow, line_size * early_fb->height);
      } else {
        /* Framebuffer too big for the shadow buffer - same as before,
         * slow on real hardware/KVM (reads the framebuffer itself to
         * shift it), fine under TCG. */
        for (int y = 0; y < (int)early_fb->height - CHAR_HEIGHT; y++)
          memcpy(fb_base + y * line_size,
                 fb_base + (y + CHAR_HEIGHT) * line_size, line_size);

        for (int y = (int)early_fb->height - CHAR_HEIGHT;
             y < (int)early_fb->height; y++)
          memset(fb_base + y * line_size, 0, line_size);
      }

      early_y = early_fb->height - CHAR_HEIGHT;
    }
    return;
  }

  if (c == '\r') {
    early_x = 0;
    return;
  }

  if (c == '\b') {
    if (early_x >= CHAR_WIDTH)
      early_x -= CHAR_WIDTH;
    return;
  }

  if (c == '\t') {
    for (int i = 0; i < 4; i++)
      early_putchar_color(' ', color);
    return;
  }

  if (early_x + CHAR_WIDTH > (int)early_fb->width)
    early_putchar_color('\n', color);

  draw_char(c, early_x, early_y, CHAR_WIDTH, CHAR_HEIGHT, color);

  if (g_fb_shadow_active) {
    /* Mirror just the glyph's own small patch into the shadow buffer -
     * same size class as the glyph draw itself (already fast), not a
     * full-row read. Assumes 4 bytes/pixel, matching textout.c's
     * (uint32_t *)fb_base access. */
    uint8_t *fb_base = (uint8_t *)early_fb->base;
    size_t line_size = early_fb->pitch;
    size_t col_off = (size_t)early_x * 4;
    size_t col_bytes = (size_t)CHAR_WIDTH * 4;

    for (int row = 0; row < CHAR_HEIGHT; row++) {
      int py = early_y + row;
      if (py < 0 || py >= (int)early_fb->height)
        continue;
      memcpy(g_fb_shadow + (size_t)py * line_size + col_off,
             fb_base + (size_t)py * line_size + col_off, col_bytes);
    }
  }

  early_x += CHAR_WIDTH;
}

/**
 * @brief Write one character in default white
 *
 * @param c Character to write
 */
void early_putchar(char c) { early_putchar_color(c, COLOR_WHITE); }

/* -- Initialisation -------------------------------------------- */

/**
 * @brief Initialise the early console with a framebuffer
 *
 * @param fb Pointer to the bootloader-provided framebuffer info
 */
void printk_init(framebuffer_info_t *fb) {
  early_fb = fb;
  early_x = 0;
  early_y = 0;
  serial_color = COLOR_WHITE;
  printk_set_output(early_putchar);
  printk_set_color_output(early_putchar_color);

  g_fb_shadow_active =
      fb && fb->base &&
      (size_t)fb->pitch * fb->height <= FB_SHADOW_MAX_BYTES;
  if (g_fb_shadow_active)
    memset(g_fb_shadow, 0, (size_t)fb->pitch * fb->height);
}
