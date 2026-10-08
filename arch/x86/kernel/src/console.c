/**
 * @file console.c
 * @brief Early serial + framebuffer console with a VT100/ANSI terminal
 *
 * Every byte goes to the serial port (COM1) unchanged, so a host
 * terminal interprets escape sequences itself. On the framebuffer the
 * console is a small VT100-style terminal: an 8x8 character-cell grid
 * with a cursor, deferred line wrap, scroll regions and the CSI
 * sequences full-screen programs (busybox vi, less, line editing) use.
 */

#include <hubble/color.h>
#include <hubble/fb.h>
#include <hubble/font.h>
#include <hubble/printk.h>
#include <hubble/string.h>
#include <io.h>
#include <stdbool.h>

/* -- Constants ------------------------------------------------- */

#define CHAR_WIDTH 8
#define CHAR_HEIGHT 8

#define CSI_MAX_PARAMS 8

/* -- Serial Output --------------------------------------------- */

static color_t serial_color = COLOR_WHITE;

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

/* -- Framebuffer and shadow ------------------------------------ */

static framebuffer_info_t *early_fb = NULL;

/* Scrolling has to know what's already on screen to shift it up a row.
 * Reading that back from the real framebuffer is fine under TCG, but on
 * real hardware and under KVM the framebuffer is typically
 * write-combined: reads from it are drastically slower than writes. So
 * every pixel is also written to this plain-RAM mirror, scrolling
 * shifts the mirror, and the result is copied to the framebuffer in one
 * sequential write pass - the cheap direction for write-combined memory.
 *
 * This runs before the memory allocator is up (printk_init() is called
 * before boot_memory_init() in main.c), so it is a static buffer sized
 * for a generous but bounded resolution. A bigger framebuffer falls
 * back to scrolling the framebuffer directly (slow but correct). */
#define FB_SHADOW_MAX_BYTES (8u * 1024 * 1024)
static uint8_t g_fb_shadow[FB_SHADOW_MAX_BYTES];
static bool g_fb_shadow_active = false;

/* -- Terminal state -------------------------------------------- */

/* What each cell shows, so the cursor can be drawn and erased over it
 * without reading the framebuffer. Sized like the shadow: one cell per
 * 8x8 pixels of an FB_SHADOW_MAX_BYTES framebuffer at 32 bpp. */
typedef struct {
  char ch;
  color_t fg;
  color_t bg;
} cell_t;

#define CELLS_MAX (FB_SHADOW_MAX_BYTES / (CHAR_WIDTH * CHAR_HEIGHT * 4))
static cell_t g_cells[CELLS_MAX];
static bool g_cells_active = false;

#define DEFAULT_FG COLOR_WHITE
#define DEFAULT_BG COLOR_BLACK

/* Standard 16-colour palette (SGR 30-37 / 90-97, 40-47 / 100-107). */
static const color_t ansi_palette[16] = {
    0xFF000000, 0xFFAA0000, 0xFF00AA00, 0xFFAA5500, 0xFF0000AA, 0xFFAA00AA,
    0xFF00AAAA, 0xFFAAAAAA, 0xFF555555, 0xFFFF5555, 0xFF55FF55, 0xFFFFFF55,
    0xFF5555FF, 0xFFFF55FF, 0xFF55FFFF, 0xFFFFFFFF,
};

typedef enum { ST_NORMAL, ST_ESC, ST_CSI, ST_SKIP_ONE } parse_state_t;

static struct {
  int cols, rows;
  int cx, cy;
  /* Writing the last column leaves the cursor there; the wrap happens
   * on the next printable byte (VT100). Otherwise a full-screen program
   * drawing its bottom-right cell would scroll the whole screen. */
  bool wrap_pending;
  int top, bottom; /* scroll region, inclusive rows */

  color_t fg, bg;
  bool bold, reverse;

  int saved_cx, saved_cy;

  bool cursor_visible;
  bool cursor_drawn;

  parse_state_t state;
  int params[CSI_MAX_PARAMS];
  int nparams;
  bool private_mode;
} term;

/* -- Pixel output ---------------------------------------------- */

static inline uint32_t *fb_row(int py) {
  return (uint32_t *)((uint8_t *)early_fb->base + (size_t)py * early_fb->pitch);
}

static inline uint32_t *shadow_row(int py) {
  return (uint32_t *)(g_fb_shadow + (size_t)py * early_fb->pitch);
}

/* Draw one glyph with an opaque background into the framebuffer (and
 * shadow). Writes only, never reads the framebuffer. */
static void render_glyph(int col, int row, char ch, color_t fg, color_t bg) {
  const uint8_t *glyph = font[(unsigned char)ch];
  int px = col * CHAR_WIDTH;
  for (int y = 0; y < CHAR_HEIGHT; y++) {
    int py = row * CHAR_HEIGHT + y;
    uint8_t bits = glyph[y];
    uint32_t line[CHAR_WIDTH];
    for (int x = 0; x < CHAR_WIDTH; x++)
      line[x] = (bits & (0x80 >> x)) ? fg : bg;
    memcpy(fb_row(py) + px, line, sizeof(line));
    if (g_fb_shadow_active)
      memcpy(shadow_row(py) + px, line, sizeof(line));
  }
}

/* Fill whole text rows [row0, row1) of pixels with one colour. */
static void fill_rows_pixels(int row0, int row1, color_t color) {
  int width = term.cols * CHAR_WIDTH;
  for (int py = row0 * CHAR_HEIGHT; py < row1 * CHAR_HEIGHT; py++) {
    /* Write both copies; never read the framebuffer back (slow). */
    uint32_t *dst = fb_row(py);
    uint32_t *shadow = g_fb_shadow_active ? shadow_row(py) : NULL;
    for (int x = 0; x < width; x++) {
      dst[x] = color;
      if (shadow)
        shadow[x] = color;
    }
  }
}

/* -- Cells ----------------------------------------------------- */

static inline cell_t *cell_at(int col, int row) {
  return &g_cells[row * term.cols + col];
}

static color_t effective_fg(void) {
  return term.reverse ? term.bg : term.fg;
}

static color_t effective_bg(void) {
  return term.reverse ? term.fg : term.bg;
}

static void set_cell(int col, int row, char ch, color_t fg, color_t bg) {
  if (g_cells_active) {
    cell_t *c = cell_at(col, row);
    c->ch = ch;
    c->fg = fg;
    c->bg = bg;
  }
  render_glyph(col, row, ch, fg, bg);
}

/* Blank cells [col0, col1) of one row in the current background. */
static void erase_cells(int row, int col0, int col1) {
  color_t bg = effective_bg();
  for (int col = col0; col < col1; col++)
    set_cell(col, row, ' ', term.fg, bg);
}

/* Blank whole rows [row0, row1). */
static void erase_rows(int row0, int row1) {
  if (row0 >= row1)
    return;
  color_t bg = effective_bg();
  if (g_cells_active) {
    for (int row = row0; row < row1; row++)
      for (int col = 0; col < term.cols; col++)
        *cell_at(col, row) = (cell_t){' ', term.fg, bg};
  }
  fill_rows_pixels(row0, row1, bg);
}

/* Move text rows [src, src+count) to dst, pixels and cells. */
static void move_rows(int dst, int src, int count) {
  if (count <= 0 || dst == src)
    return;
  size_t row_bytes = (size_t)early_fb->pitch * CHAR_HEIGHT;
  uint8_t *base = g_fb_shadow_active ? g_fb_shadow : (uint8_t *)early_fb->base;
  memmove(base + dst * row_bytes, base + src * row_bytes, count * row_bytes);
  if (g_fb_shadow_active)
    memcpy((uint8_t *)early_fb->base + dst * row_bytes,
           g_fb_shadow + dst * row_bytes, count * row_bytes);
  if (g_cells_active)
    memmove(cell_at(0, dst), cell_at(0, src),
            (size_t)count * term.cols * sizeof(cell_t));
}

/* Scroll the region [top, bottom] up by n rows (text moves up). */
static void scroll_up(int top, int bottom, int n) {
  int height = bottom - top + 1;
  if (n > height)
    n = height;
  move_rows(top, top + n, height - n);
  erase_rows(bottom - n + 1, bottom + 1);
}

/* Scroll the region [top, bottom] down by n rows (text moves down). */
static void scroll_down(int top, int bottom, int n) {
  int height = bottom - top + 1;
  if (n > height)
    n = height;
  move_rows(top + n, top, height - n);
  erase_rows(top, top + n);
}

/* -- Cursor ---------------------------------------------------- */

static void draw_cursor(bool inverted) {
  if (!g_cells_active)
    return;
  int col = term.cx < term.cols ? term.cx : term.cols - 1;
  const cell_t *c = cell_at(col, term.cy);
  if (inverted)
    render_glyph(col, term.cy, c->ch, c->bg, c->fg);
  else
    render_glyph(col, term.cy, c->ch, c->fg, c->bg);
}

static void cursor_hide(void) {
  if (term.cursor_drawn) {
    draw_cursor(false);
    term.cursor_drawn = false;
  }
}

static void cursor_show(void) {
  if (term.cursor_visible && !term.cursor_drawn) {
    draw_cursor(true);
    term.cursor_drawn = true;
  }
}

/* -- Cursor movement ------------------------------------------- */

static int clamp(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

/* LF: down one row, scrolling the region when at its bottom margin. */
static void line_feed(void) {
  if (term.cy == term.bottom)
    scroll_up(term.top, term.bottom, 1);
  else if (term.cy < term.rows - 1)
    term.cy++;
}

/* RI: up one row, scrolling the region down when at its top margin. */
static void reverse_index(void) {
  if (term.cy == term.top)
    scroll_down(term.top, term.bottom, 1);
  else if (term.cy > 0)
    term.cy--;
}

static void put_printable(char c, color_t color_override) {
  if (term.wrap_pending) {
    term.cx = 0;
    line_feed();
    term.wrap_pending = false;
  }
  color_t fg = color_override != COLOR_WHITE ? color_override : effective_fg();
  set_cell(term.cx, term.cy, c, fg, effective_bg());
  if (term.cx == term.cols - 1)
    term.wrap_pending = true;
  else
    term.cx++;
}

/* -- Escape sequences ------------------------------------------ */

static void reset_attributes(void) {
  term.fg = DEFAULT_FG;
  term.bg = DEFAULT_BG;
  term.bold = false;
  term.reverse = false;
}

/* Parameter i, with 0 / missing meaning "default". */
static int param(int i, int def) {
  return (i < term.nparams && term.params[i] > 0) ? term.params[i] : def;
}

static void apply_sgr(void) {
  if (term.nparams == 0) {
    reset_attributes();
    return;
  }
  for (int i = 0; i < term.nparams; i++) {
    int p = term.params[i];
    if (p == 0)
      reset_attributes();
    else if (p == 1)
      term.bold = true;
    else if (p == 22)
      term.bold = false;
    else if (p == 7)
      term.reverse = true;
    else if (p == 27)
      term.reverse = false;
    else if (p >= 30 && p <= 37)
      term.fg = ansi_palette[p - 30 + (term.bold ? 8 : 0)];
    else if (p == 39)
      term.fg = DEFAULT_FG;
    else if (p >= 40 && p <= 47)
      term.bg = ansi_palette[p - 40];
    else if (p == 49)
      term.bg = DEFAULT_BG;
    else if (p >= 90 && p <= 97)
      term.fg = ansi_palette[p - 90 + 8];
    else if (p >= 100 && p <= 107)
      term.bg = ansi_palette[p - 100 + 8];
  }
}

static void erase_display(int mode) {
  if (mode == 0) { /* cursor to end */
    erase_cells(term.cy, term.cx, term.cols);
    erase_rows(term.cy + 1, term.rows);
  } else if (mode == 1) { /* start to cursor */
    erase_rows(0, term.cy);
    erase_cells(term.cy, 0, clamp(term.cx + 1, 0, term.cols));
  } else { /* 2, 3: everything */
    erase_rows(0, term.rows);
  }
}

static void erase_line(int mode) {
  if (mode == 0)
    erase_cells(term.cy, term.cx, term.cols);
  else if (mode == 1)
    erase_cells(term.cy, 0, clamp(term.cx + 1, 0, term.cols));
  else
    erase_cells(term.cy, 0, term.cols);
}

/* Shift the rest of the line right by n (ICH) or left by n (DCH). */
static void shift_line(int n, bool insert) {
  if (!g_cells_active)
    return;
  int row = term.cy;
  n = clamp(n, 0, term.cols - term.cx);
  if (insert) {
    for (int col = term.cols - 1; col >= term.cx + n; col--) {
      cell_t c = *cell_at(col - n, row);
      set_cell(col, row, c.ch, c.fg, c.bg);
    }
    erase_cells(row, term.cx, term.cx + n);
  } else {
    for (int col = term.cx; col < term.cols - n; col++) {
      cell_t c = *cell_at(col + n, row);
      set_cell(col, row, c.ch, c.fg, c.bg);
    }
    erase_cells(row, term.cols - n, term.cols);
  }
}

static void set_private_mode(bool enable) {
  for (int i = 0; i < term.nparams; i++) {
    switch (term.params[i]) {
    case 25: /* cursor visibility */
      term.cursor_visible = enable;
      break;
    case 47:
    case 1047:
    case 1049: /* alternate screen: no second buffer, start clean */
      if (enable) {
        term.saved_cx = term.cx;
        term.saved_cy = term.cy;
      }
      erase_rows(0, term.rows);
      if (!enable) {
        term.cx = term.saved_cx;
        term.cy = term.saved_cy;
      }
      break;
    }
  }
}

static void csi_dispatch(char final) {
  if (term.private_mode) {
    if (final == 'h' || final == 'l')
      set_private_mode(final == 'h');
    return;
  }

  int last_col = term.cols - 1, last_row = term.rows - 1;
  term.wrap_pending = false;

  switch (final) {
  case 'A':
    term.cy = clamp(term.cy - param(0, 1), 0, last_row);
    break;
  case 'B':
  case 'e':
    term.cy = clamp(term.cy + param(0, 1), 0, last_row);
    break;
  case 'C':
  case 'a':
    term.cx = clamp(term.cx + param(0, 1), 0, last_col);
    break;
  case 'D':
    term.cx = clamp(term.cx - param(0, 1), 0, last_col);
    break;
  case 'E':
    term.cy = clamp(term.cy + param(0, 1), 0, last_row);
    term.cx = 0;
    break;
  case 'F':
    term.cy = clamp(term.cy - param(0, 1), 0, last_row);
    term.cx = 0;
    break;
  case 'G':
  case '`':
    term.cx = clamp(param(0, 1) - 1, 0, last_col);
    break;
  case 'd':
    term.cy = clamp(param(0, 1) - 1, 0, last_row);
    break;
  case 'H':
  case 'f':
    term.cy = clamp(param(0, 1) - 1, 0, last_row);
    term.cx = clamp(param(1, 1) - 1, 0, last_col);
    break;
  case 'J':
    erase_display(term.nparams ? term.params[0] : 0);
    break;
  case 'K':
    erase_line(term.nparams ? term.params[0] : 0);
    break;
  case 'X':
    erase_cells(term.cy, term.cx, clamp(term.cx + param(0, 1), 0, term.cols));
    break;
  case '@':
    shift_line(param(0, 1), true);
    break;
  case 'P':
    shift_line(param(0, 1), false);
    break;
  case 'L': /* insert lines at the cursor, within the scroll region */
    if (term.cy >= term.top && term.cy <= term.bottom)
      scroll_down(term.cy, term.bottom, param(0, 1));
    break;
  case 'M': /* delete lines at the cursor, within the scroll region */
    if (term.cy >= term.top && term.cy <= term.bottom)
      scroll_up(term.cy, term.bottom, param(0, 1));
    break;
  case 'S':
    scroll_up(term.top, term.bottom, param(0, 1));
    break;
  case 'T':
    scroll_down(term.top, term.bottom, param(0, 1));
    break;
  case 'm':
    apply_sgr();
    break;
  case 'r': { /* scroll region; cursor goes home */
    int top = param(0, 1) - 1, bottom = param(1, term.rows) - 1;
    if (top < bottom && bottom <= last_row) {
      term.top = top;
      term.bottom = bottom;
    }
    term.cx = 0;
    term.cy = 0;
    break;
  }
  case 's':
    term.saved_cx = term.cx;
    term.saved_cy = term.cy;
    break;
  case 'u':
    term.cx = term.saved_cx;
    term.cy = term.saved_cy;
    break;
  default: /* unsupported (e.g. DSR 'n', DA 'c'): ignored */
    break;
  }
}

static void terminal_reset(void) {
  reset_attributes();
  term.cx = term.cy = 0;
  term.wrap_pending = false;
  term.top = 0;
  term.bottom = term.rows - 1;
  term.saved_cx = term.saved_cy = 0;
  term.cursor_visible = true;
  term.state = ST_NORMAL;
  erase_rows(0, term.rows);
}

static void term_feed(char c, color_t color) {
  switch (term.state) {
  case ST_NORMAL:
    break;

  case ST_ESC:
    term.state = ST_NORMAL;
    switch (c) {
    case '[':
      term.state = ST_CSI;
      term.nparams = 0;
      term.params[0] = 0;
      term.private_mode = false;
      return;
    case '7':
      term.saved_cx = term.cx;
      term.saved_cy = term.cy;
      return;
    case '8':
      term.cx = term.saved_cx;
      term.cy = term.saved_cy;
      term.wrap_pending = false;
      return;
    case 'D':
      line_feed();
      return;
    case 'E':
      term.cx = 0;
      line_feed();
      return;
    case 'M':
      reverse_index();
      return;
    case 'c':
      terminal_reset();
      return;
    case '(':
    case ')': /* character set designation: one more byte, ignored */
      term.state = ST_SKIP_ONE;
      return;
    default:
      return;
    }

  case ST_SKIP_ONE:
    term.state = ST_NORMAL;
    return;

  case ST_CSI:
    if (c == '?' && term.nparams == 0 && term.params[0] == 0) {
      term.private_mode = true;
    } else if (c >= '0' && c <= '9') {
      if (term.nparams == 0)
        term.nparams = 1;
      int *p = &term.params[term.nparams - 1];
      if (*p < 10000)
        *p = *p * 10 + (c - '0');
    } else if (c == ';') {
      if (term.nparams == 0)
        term.nparams = 1;
      if (term.nparams < CSI_MAX_PARAMS)
        term.params[term.nparams++] = 0;
    } else if (c >= 0x40 && c <= 0x7E) {
      term.state = ST_NORMAL;
      csi_dispatch(c);
    } else if (c == 0x1B) {
      term.state = ST_ESC; /* aborted sequence */
    }
    /* intermediates (0x20-0x2F) and stray controls: ignored */
    return;
  }

  /* ST_NORMAL */
  switch (c) {
  case 0x1B:
    term.state = ST_ESC;
    return;
  case '\n': /* NL also returns the carriage (printk relies on it) */
    term.cx = 0;
    term.wrap_pending = false;
    line_feed();
    return;
  case '\r':
    term.cx = 0;
    term.wrap_pending = false;
    return;
  case '\b':
    if (term.cx > 0 && !term.wrap_pending)
      term.cx--;
    term.wrap_pending = false;
    return;
  case '\t': {
    int next = (term.cx / 8 + 1) * 8;
    term.cx = next < term.cols ? next : term.cols - 1;
    return;
  }
  default:
    if ((unsigned char)c < 0x20 || c == 0x7F)
      return; /* BEL and other controls: no glyph */
    put_printable(c, color);
  }
}

/* -- Public interface ------------------------------------------ */

/**
 * @brief Write one character to both serial and framebuffer
 *
 * @param c     Character to write
 * @param color Colour override for this character (printk levels);
 *              COLOR_WHITE means "use the terminal's current colour"
 */
void early_putchar_color(char c, color_t color) {
  serial_set_color(color);
  serial_putc(c);

  if (!early_fb || !early_fb->base || term.cols == 0)
    return;

  cursor_hide();
  term_feed(c, color);
  cursor_show();
}

/**
 * @brief Write one character in default white
 *
 * @param c Character to write
 */
void early_putchar(char c) { early_putchar_color(c, COLOR_WHITE); }

/** @brief Clear the framebuffer console and home the cursor. */
void console_clear(void) {
  if (!early_fb || term.cols == 0)
    return;
  cursor_hide();
  erase_rows(0, term.rows);
  term.cx = term.cy = 0;
  term.wrap_pending = false;
  cursor_show();
}

/** @brief Text grid size of the framebuffer console (80x25 without one). */
void console_get_size(uint16_t *cols, uint16_t *rows) {
  *cols = term.cols ? (uint16_t)term.cols : 80;
  *rows = term.rows ? (uint16_t)term.rows : 25;
}

/* -- Initialisation -------------------------------------------- */

/**
 * @brief Initialise the early console with a framebuffer
 *
 * @param fb Pointer to the bootloader-provided framebuffer info
 */
void printk_init(framebuffer_info_t *fb) {
  early_fb = fb;
  serial_color = COLOR_WHITE;
  printk_set_output(early_putchar);
  printk_set_color_output(early_putchar_color);

  if (!fb || !fb->base || fb->bpp != 32) {
    term.cols = term.rows = 0; /* serial only */
    return;
  }

  term.cols = fb->width / CHAR_WIDTH;
  term.rows = fb->height / CHAR_HEIGHT;

  g_fb_shadow_active = (size_t)fb->pitch * fb->height <= FB_SHADOW_MAX_BYTES;
  g_cells_active = (size_t)term.cols * term.rows <= CELLS_MAX;

  terminal_reset();
  cursor_show();
}
