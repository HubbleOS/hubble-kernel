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

#include <higher_half.h>
#include <hubble/color.h>
#include <hubble/fb.h>
#include <hubble/font.h>
#include <hubble/printk.h>
#include <hubble/string.h>
#include <io.h>
#include <mm/pmm.h>
#include <smp/spinlock.h>
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

/* -- Console surface ------------------------------------------- */

static framebuffer_info_t *early_fb = NULL;

/* The console draws only into its own surface: plain RAM, one 32-bit
 * pixel per dot, stride = width. present() copies the changed rectangle
 * to the framebuffer at (origin_x, origin_y) - but only while the
 * console owns the display. When a graphical program takes the display
 * (KDSETMODE KD_GRAPHICS) the console keeps drawing into its surface,
 * so nothing is lost and nothing scribbles over the program; taking the
 * display back is one full copy.
 *
 * Scrolling shifts the surface, never the framebuffer: framebuffers are
 * typically write-combined, where reads are drastically slower than
 * writes, so the framebuffer is only ever written, sequentially.
 *
 * printk_init() runs before the memory allocator (boot_memory_init() in
 * main.c), so the console starts on static buffers; if they can't hold
 * the whole screen it uses the top rows only, and console_init_late()
 * moves it to full-size buffers once pages can be allocated. */
#define SURFACE_EARLY_BYTES (8u * 1024 * 1024)
static uint32_t g_surface_early[SURFACE_EARLY_BYTES / sizeof(uint32_t)];

static struct {
  uint32_t *pixels;
  int width, height;
  int origin_x, origin_y; /* top-left corner on the framebuffer */
  bool presenting;        /* the console owns the display */
  int dirty_x0, dirty_y0, dirty_x1, dirty_y1; /* empty if x0 >= x1 */
} surface;

/* Console state is shared by printk (any CPU, IRQ context) and the tty
 * mode switch. */
static irqlock_t console_lock = IRQLOCK_INIT("console");

/* -- Terminal state -------------------------------------------- */

/* What each cell shows, so the cursor can be drawn and erased over it
 * and a scroll can move text without reading pixels back. */
typedef struct {
  char ch;
  color_t fg;
  color_t bg;
} cell_t;

#define CELLS_EARLY (SURFACE_EARLY_BYTES / (CHAR_WIDTH * CHAR_HEIGHT * 4))
static cell_t g_cells_early[CELLS_EARLY];
static cell_t *g_cells = g_cells_early;

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

static inline uint32_t *surface_row(int py) {
  return surface.pixels + (size_t)py * surface.width;
}

static void mark_dirty(int x0, int y0, int x1, int y1) {
  if (surface.dirty_x0 >= surface.dirty_x1) {
    surface.dirty_x0 = x0;
    surface.dirty_y0 = y0;
    surface.dirty_x1 = x1;
    surface.dirty_y1 = y1;
    return;
  }
  if (x0 < surface.dirty_x0)
    surface.dirty_x0 = x0;
  if (y0 < surface.dirty_y0)
    surface.dirty_y0 = y0;
  if (x1 > surface.dirty_x1)
    surface.dirty_x1 = x1;
  if (y1 > surface.dirty_y1)
    surface.dirty_y1 = y1;
}

/* Copy the dirty rectangle to the framebuffer, clipped to it. */
static void present(void) {
  int x0 = surface.dirty_x0, y0 = surface.dirty_y0;
  int x1 = surface.dirty_x1, y1 = surface.dirty_y1;
  if (x0 >= x1 || !surface.presenting)
    return;
  surface.dirty_x0 = surface.dirty_x1 = 0;

  int fb_w = (int)early_fb->width, fb_h = (int)early_fb->height;
  int fx0 = surface.origin_x + x0, fx1 = surface.origin_x + x1;
  if (fx0 < 0) {
    x0 -= fx0;
    fx0 = 0;
  }
  if (fx1 > fb_w)
    fx1 = fb_w;
  if (fx0 >= fx1)
    return;

  for (int py = y0; py < y1; py++) {
    int fy = surface.origin_y + py;
    if (fy < 0 || fy >= fb_h)
      continue;
    uint8_t *dst = (uint8_t *)early_fb->base + (size_t)fy * early_fb->pitch;
    memcpy((uint32_t *)dst + fx0, surface_row(py) + x0,
           (size_t)(fx1 - fx0) * sizeof(uint32_t));
  }
}

static void mark_all_dirty(void) {
  mark_dirty(0, 0, surface.width, surface.height);
}

/* Draw one glyph with an opaque background. */
static void render_glyph(int col, int row, char ch, color_t fg, color_t bg) {
  const uint8_t *glyph = font[(unsigned char)ch];
  int px = col * CHAR_WIDTH;
  for (int y = 0; y < CHAR_HEIGHT; y++) {
    uint32_t *line = surface_row(row * CHAR_HEIGHT + y) + px;
    uint8_t bits = glyph[y];
    for (int x = 0; x < CHAR_WIDTH; x++)
      line[x] = (bits & (0x80 >> x)) ? fg : bg;
  }
  mark_dirty(px, row * CHAR_HEIGHT, px + CHAR_WIDTH, (row + 1) * CHAR_HEIGHT);
}

/* Fill whole text rows [row0, row1) of pixels with one colour. */
static void fill_rows_pixels(int row0, int row1, color_t color) {
  for (int py = row0 * CHAR_HEIGHT; py < row1 * CHAR_HEIGHT; py++) {
    uint32_t *line = surface_row(py);
    for (int x = 0; x < surface.width; x++)
      line[x] = color;
  }
  mark_dirty(0, row0 * CHAR_HEIGHT, surface.width, row1 * CHAR_HEIGHT);
}

/* -- Cells ----------------------------------------------------- */

static inline cell_t *cell_at(int col, int row) {
  return &g_cells[row * term.cols + col];
}

static color_t effective_fg(void) { return term.reverse ? term.bg : term.fg; }

static color_t effective_bg(void) { return term.reverse ? term.fg : term.bg; }

static void set_cell(int col, int row, char ch, color_t fg, color_t bg) {
  cell_t *c = cell_at(col, row);
  c->ch = ch;
  c->fg = fg;
  c->bg = bg;
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
  for (int row = row0; row < row1; row++)
    for (int col = 0; col < term.cols; col++)
      *cell_at(col, row) = (cell_t){' ', term.fg, bg};
  fill_rows_pixels(row0, row1, bg);
}

/* Move text rows [src, src+count) to dst, pixels and cells. */
static void move_rows(int dst, int src, int count) {
  if (count <= 0 || dst == src)
    return;
  size_t row_pixels = (size_t)surface.width * CHAR_HEIGHT;
  memmove(surface.pixels + dst * row_pixels, surface.pixels + src * row_pixels,
          count * row_pixels * sizeof(uint32_t));
  memmove(cell_at(0, dst), cell_at(0, src),
          (size_t)count * term.cols * sizeof(cell_t));
  mark_dirty(0, dst * CHAR_HEIGHT, surface.width, (dst + count) * CHAR_HEIGHT);
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

static int clamp(int v, int lo, int hi) {
  return v < lo ? lo : v > hi ? hi : v;
}

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
 * @brief Write one character to serial and, optionally, the framebuffer
 *
 * @param c      Character to write
 * @param color  Colour override for this character (printk levels);
 *               COLOR_WHITE means "use the terminal's current colour"
 * @param screen Also draw it on the framebuffer console (printk leaves
 *               messages below the console log level off the screen)
 */
void early_putchar_color(char c, color_t color, bool screen) {
  serial_set_color(color);
  serial_putc(c);

  if (!screen || term.cols == 0)
    return;

  irqlock_acquire(&console_lock);
  cursor_hide();
  term_feed(c, color);
  cursor_show();
  present();
  irqlock_release(&console_lock);
}

/**
 * @brief Write one character in default white
 *
 * @param c Character to write
 */
void early_putchar(char c) { early_putchar_color(c, COLOR_WHITE, true); }

/** @brief Clear the framebuffer console and home the cursor. */
void console_clear(void) {
  if (term.cols == 0)
    return;
  irqlock_acquire(&console_lock);
  cursor_hide();
  erase_rows(0, term.rows);
  term.cx = term.cy = 0;
  term.wrap_pending = false;
  cursor_show();
  present();
  irqlock_release(&console_lock);
}

/** @brief Text grid size of the framebuffer console (80x25 without one). */
void console_get_size(uint16_t *cols, uint16_t *rows) {
  *cols = term.cols ? (uint16_t)term.cols : 80;
  *rows = term.rows ? (uint16_t)term.rows : 25;
}

void console_set_output(bool enabled) {
  if (term.cols == 0)
    return;
  irqlock_acquire(&console_lock);
  if (enabled && !surface.presenting) {
    surface.presenting = true;
    mark_all_dirty(); /* the display holds someone else's pixels */
    present();
  }
  surface.presenting = enabled;
  irqlock_release(&console_lock);
}

bool console_output_enabled(void) { return surface.presenting; }

void console_set_origin(int x, int y) {
  if (term.cols == 0)
    return;
  irqlock_acquire(&console_lock);
  surface.origin_x = x;
  surface.origin_y = y;
  mark_all_dirty();
  present();
  irqlock_release(&console_lock);
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

  if (!fb || !fb->base || fb->bpp != 32 || fb->width < CHAR_WIDTH ||
      fb->height < CHAR_HEIGHT) {
    term.cols = term.rows = 0; /* serial only */
    return;
  }

  /* As many rows as the static buffers hold; console_init_late() grows
   * them to the full screen. */
  int cols = fb->width / CHAR_WIDTH;
  int rows = fb->height / CHAR_HEIGHT;
  int rows_fit = (int)(CELLS_EARLY / cols);
  term.cols = cols;
  term.rows = rows < rows_fit ? rows : rows_fit;

  surface.pixels = g_surface_early;
  surface.width = term.cols * CHAR_WIDTH;
  surface.height = term.rows * CHAR_HEIGHT;
  surface.presenting = true;

  terminal_reset();
  cursor_show();
  present();
}

/**
 * @brief Move the console to buffers sized for the whole screen
 *
 * Needs the page allocator. Only does work when the screen was too big
 * for the static buffers; existing text and the cursor stay put.
 */
void console_init_late(void) {
  if (term.cols == 0)
    return;

  int rows = (int)early_fb->height / CHAR_HEIGHT;
  if (rows <= term.rows)
    return;

  size_t pixel_bytes = (size_t)surface.width * rows * CHAR_HEIGHT * 4;
  size_t cell_bytes = (size_t)term.cols * rows * sizeof(cell_t);
  uint64_t pixels_phys = pmm_alloc_pages((pixel_bytes + 4095) / 4096);
  uint64_t cells_phys = pmm_alloc_pages((cell_bytes + 4095) / 4096);
  if (!pixels_phys || !cells_phys) {
    if (pixels_phys)
      pmm_free_pages(pixels_phys, (pixel_bytes + 4095) / 4096);
    if (cells_phys)
      pmm_free_pages(cells_phys, (cell_bytes + 4095) / 4096);
    printk(KERN_WARNING "console: no memory for a full-screen buffer\n");
    return;
  }

  irqlock_acquire(&console_lock);

  /* Same width, so the old contents are a prefix of the new layout. */
  uint32_t *pixels = (uint32_t *)phys_to_virt(pixels_phys);
  cell_t *cells = (cell_t *)phys_to_virt(cells_phys);
  memcpy(pixels, surface.pixels, (size_t)surface.width * surface.height * 4);
  memcpy(cells, g_cells, (size_t)term.cols * term.rows * sizeof(cell_t));

  int old_rows = term.rows;
  surface.pixels = pixels;
  surface.height = rows * CHAR_HEIGHT;
  g_cells = cells;
  term.rows = rows;
  if (term.bottom == old_rows - 1)
    term.bottom = rows - 1;

  erase_rows(old_rows, rows);
  present();

  irqlock_release(&console_lock);
}
