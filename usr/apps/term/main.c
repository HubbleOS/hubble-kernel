/*
 * term - a terminal emulator, as a libgui application.
 *
 * Runs /bin/sh on a pseudo-terminal and shows what it writes in a window,
 * interpreting the escape sequences of TERM=linux (cursor motion, erasing,
 * scroll regions, colours, the line-drawing set) plus the xterm ones
 * programs send anyway (the alternate screen, 256 colours). Keys typed
 * into the window go to the shell. The window closes when the shell exits.
 *
 * The panel's "Terminal" button starts one; so does running /bin/term
 * while /bin/desktop is up.
 */
#define _XOPEN_SOURCE 600 /* posix_openpt */
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include <libgui.h>
#include <ui/text/fonts/font.h>

#define SHELL "/bin/sh"

#define COLS 80
#define ROWS 24
#define CELL_W 8
#define CELL_H 12 /* the 8x8 font, with 2 px above and below */
#define PAD 4     /* around the text */

#define DEFAULT_FG 7
#define DEFAULT_BG 0

/* -- The screen ------------------------------------------------------------ */

enum { ATTR_BOLD = 1, ATTR_REVERSE = 2, ATTR_UNDERLINE = 4 };

typedef struct {
  char ch;
  uint8_t attr;
  uint32_t fg, bg; /* palette index (< 256) or 0xRRGGBB | DIRECT */
} cell_t;

#define DIRECT 0x1000000u

typedef struct {
  cell_t cells[ROWS][COLS];
  int x, y;
} grid_t;

static grid_t screens[2]; /* main, alternate */
static grid_t *grid = &screens[0];

static cell_t pen = {' ', 0, DEFAULT_FG, DEFAULT_BG}; /* what is written */
static int top, bottom = ROWS - 1; /* scroll region */
static bool wrap_next;             /* at the last column: wrap first */
static bool autowrap = true;
static bool cursor_visible = true;
static bool app_cursor; /* arrows send ESC O x */
static bool insert_mode;

/* Character sets: G0/G1, each ASCII or DEC line drawing; SO picks G1. */
static bool charset_g[2];
static int charset; /* 0 or 1 */
static bool acs_sgr; /* SGR 11 (linux) also selects line drawing */

static struct {
  int x, y;
  cell_t pen;
  bool charset_g[2];
  int charset;
} saved;

static bool dirty[ROWS];
static int cursor_drawn_y = -1; /* row the cursor was last drawn in */

static void mark(int y) {
  if (y >= 0 && y < ROWS)
    dirty[y] = true;
}

static void mark_all(void) {
  for (int y = 0; y < ROWS; y++)
    dirty[y] = true;
}

static cell_t blank(void) {
  /* Erasing fills with the background colour in use (as xterm does). */
  return (cell_t){' ', 0, pen.fg, pen.attr & ATTR_REVERSE ? pen.fg : pen.bg};
}

static void clear_cells(int y, int x0, int x1) {
  for (int x = x0; x < x1 && x < COLS; x++)
    grid->cells[y][x] = blank();
  mark(y);
}

/* Move rows [t, b] up by n (new blank rows at the bottom), or down when
 * n < 0. */
static void scroll(int t, int b, int n) {
  if (t > b)
    return;
  int span = b - t + 1;
  if (n > span)
    n = span;
  if (n < -span)
    n = -span;
  if (n > 0) {
    memmove(grid->cells[t], grid->cells[t + n],
            (span - n) * sizeof(grid->cells[0]));
    for (int y = b - n + 1; y <= b; y++)
      clear_cells(y, 0, COLS);
  } else if (n < 0) {
    n = -n;
    memmove(grid->cells[t + n], grid->cells[t],
            (span - n) * sizeof(grid->cells[0]));
    for (int y = t; y < t + n; y++)
      clear_cells(y, 0, COLS);
  }
  for (int y = t; y <= b; y++)
    mark(y);
}

static void move_to(int x, int y) {
  grid->x = x < 0 ? 0 : x >= COLS ? COLS - 1 : x;
  grid->y = y < 0 ? 0 : y >= ROWS ? ROWS - 1 : y;
  wrap_next = false;
}

static void linefeed(void) {
  if (grid->y == bottom)
    scroll(top, bottom, 1);
  else if (grid->y < ROWS - 1)
    grid->y++;
  wrap_next = false;
}

static void reverse_index(void) {
  if (grid->y == top)
    scroll(top, bottom, -1);
  else if (grid->y > 0)
    grid->y--;
  wrap_next = false;
}

static void reset(void) {
  screens[1] = screens[0] = (grid_t){0};
  grid = &screens[0];
  pen = (cell_t){' ', 0, DEFAULT_FG, DEFAULT_BG};
  for (int i = 0; i < 2; i++)
    for (int y = 0; y < ROWS; y++)
      for (int x = 0; x < COLS; x++)
        screens[i].cells[y][x] = pen;
  top = 0, bottom = ROWS - 1;
  wrap_next = false;
  autowrap = cursor_visible = true;
  app_cursor = insert_mode = false;
  charset_g[0] = charset_g[1] = false;
  charset = 0;
  acs_sgr = false;
  saved.pen = pen;
  mark_all();
}

static void save_cursor(void) {
  saved.x = grid->x, saved.y = grid->y, saved.pen = pen;
  memcpy(saved.charset_g, charset_g, sizeof(charset_g));
  saved.charset = charset;
}

static void restore_cursor(void) {
  move_to(saved.x, saved.y);
  pen = saved.pen;
  memcpy(charset_g, saved.charset_g, sizeof(charset_g));
  charset = saved.charset;
}

/* -- Characters ------------------------------------------------------------ */

/* DEC special graphics, drawn with what the ASCII font has. */
static char line_drawing(char c) {
  switch (c) {
  case 'j': case 'k': case 'l': case 'm': case 'n':
  case 't': case 'u': case 'v': case 'w': return '+';
  case 'q': case 'o': case 'p': case 'r': case 's': return '-';
  case 'x': return '|';
  case '`': return '*'; /* diamond */
  case 'a': return '#'; /* checkerboard */
  case 'f': return '\''; /* degree */
  case 'g': return '#'; /* plus/minus */
  case '~': return '.'; /* bullet */
  case 'y': return '<';
  case 'z': return '>';
  case '{': return '*'; /* pi */
  case '|': return '!'; /* not equal */
  case '}': return '#'; /* pound */
  case '0': return '#'; /* block */
  default: return c;
  }
}

/* A Unicode character in the ASCII font: box drawing as lines, the rest
 * as '?'. */
static char from_unicode(uint32_t cp) {
  if (cp < 128)
    return cp;
  if (cp == 0xA0)
    return ' ';
  if (cp >= 0x2500 && cp <= 0x257F) {
    switch (cp) {
    case 0x2500: case 0x2501: case 0x2504: case 0x2505: case 0x2508:
    case 0x2509: case 0x254C: case 0x254D: case 0x2550: case 0x2574:
    case 0x2576: case 0x2578: case 0x257A: case 0x257C: case 0x257E:
      return '-';
    case 0x2502: case 0x2503: case 0x2506: case 0x2507: case 0x250A:
    case 0x250B: case 0x254E: case 0x254F: case 0x2551: case 0x2575:
    case 0x2577: case 0x2579: case 0x257B: case 0x257D: case 0x257F:
      return '|';
    default:
      return '+';
    }
  }
  if (cp >= 0x2580 && cp <= 0x259F)
    return '#'; /* blocks */
  if (cp == 0x2022 || cp == 0x00B7)
    return '.';
  return '?';
}

static void put_char(char c) {
  if (charset_g[charset] || acs_sgr)
    c = line_drawing(c);

  if (wrap_next) {
    grid->x = 0;
    linefeed();
  }
  cell_t *row = grid->cells[grid->y];
  if (insert_mode)
    memmove(&row[grid->x + 1], &row[grid->x],
            (COLS - grid->x - 1) * sizeof(cell_t));
  row[grid->x] = pen;
  row[grid->x].ch = c;
  mark(grid->y);

  if (grid->x == COLS - 1)
    wrap_next = autowrap;
  else
    grid->x++;
}

/* -- Escape sequences ------------------------------------------------------ */

static enum { GROUND, ESC, CSI, OSC, OSC_ESC, CHARSET } state;
static int charset_slot; /* CHARSET: '(' G0 or ')' G1 */

#define MAX_PARAMS 16
static int params[MAX_PARAMS];
static int nparams;
static bool private_mode; /* CSI ? ... */
static char intermediate; /* CSI ... ' ' q etc. */

/* UTF-8 being decoded. */
static uint32_t utf8_cp;
static int utf8_left;

static int master = -1; /* the pty, our side */

static void reply(const char *s) {
  size_t len = strlen(s);
  while (len) {
    ssize_t n = write(master, s, len);
    if (n < 0 && errno == EINTR)
      continue;
    if (n <= 0)
      return;
    s += n;
    len -= n;
  }
}

static int param(int i, int def) {
  return i < nparams && params[i] > 0 ? params[i] : def;
}

static void sgr(void) {
  if (nparams == 0)
    nparams = 1, params[0] = 0;
  for (int i = 0; i < nparams; i++) {
    int p = params[i];
    if (p == 0) {
      pen.attr = 0, pen.fg = DEFAULT_FG, pen.bg = DEFAULT_BG;
      acs_sgr = false;
    } else if (p == 1) {
      pen.attr |= ATTR_BOLD;
    } else if (p == 4) {
      pen.attr |= ATTR_UNDERLINE;
    } else if (p == 7) {
      pen.attr |= ATTR_REVERSE;
    } else if (p == 10) {
      acs_sgr = false;
    } else if (p == 11) {
      acs_sgr = true;
    } else if (p == 21 || p == 22) {
      pen.attr &= ~ATTR_BOLD;
    } else if (p == 24) {
      pen.attr &= ~ATTR_UNDERLINE;
    } else if (p == 27) {
      pen.attr &= ~ATTR_REVERSE;
    } else if (p >= 30 && p <= 37) {
      pen.fg = p - 30;
    } else if (p == 39) {
      pen.fg = DEFAULT_FG;
    } else if (p >= 40 && p <= 47) {
      pen.bg = p - 40;
    } else if (p == 49) {
      pen.bg = DEFAULT_BG;
    } else if (p >= 90 && p <= 97) {
      pen.fg = p - 90 + 8;
    } else if (p >= 100 && p <= 107) {
      pen.bg = p - 100 + 8;
    } else if ((p == 38 || p == 48) && i + 1 < nparams) {
      /* 38;5;n (256 colours) or 38;2;r;g;b */
      uint32_t color;
      if (params[i + 1] == 5 && i + 2 < nparams) {
        color = params[i + 2] & 0xFF;
        i += 2;
      } else if (params[i + 1] == 2 && i + 4 < nparams) {
        color = DIRECT | (params[i + 2] & 0xFF) << 16 |
                (params[i + 3] & 0xFF) << 8 | (params[i + 4] & 0xFF);
        i += 4;
      } else {
        break;
      }
      if (p == 38)
        pen.fg = color;
      else
        pen.bg = color;
    }
  }
}

static void set_mode(bool on) {
  for (int i = 0; i < nparams; i++) {
    int p = params[i];
    if (!private_mode) {
      if (p == 4)
        insert_mode = on;
      continue;
    }
    switch (p) {
    case 1: app_cursor = on; break;
    case 7: autowrap = on; break;
    case 25: cursor_visible = on; mark(grid->y); break;
    case 47: case 1047: case 1049:
      if (on == (grid == &screens[1]))
        break;
      if (p == 1049 && on)
        save_cursor();
      if (on) {
        grid = &screens[1];
        for (int y = 0; y < ROWS; y++)
          clear_cells(y, 0, COLS);
        grid->x = screens[0].x, grid->y = screens[0].y;
      } else {
        grid = &screens[0];
      }
      if (p == 1049 && !on)
        restore_cursor();
      wrap_next = false;
      mark_all();
      break;
    }
  }
}

static void csi(char final) {
  int n = param(0, 1);
  int x = grid->x, y = grid->y;

  if (intermediate) /* cursor style (SP q) and the like: nothing to do */
    return;

  switch (final) {
  case '@': /* insert blanks */
    if (n > COLS - x)
      n = COLS - x;
    memmove(&grid->cells[y][x + n], &grid->cells[y][x],
            (COLS - x - n) * sizeof(cell_t));
    clear_cells(y, x, x + n);
    break;
  case 'A': move_to(x, y - n < top && y >= top ? top : y - n); break;
  case 'B': case 'e':
    move_to(x, y + n > bottom && y <= bottom ? bottom : y + n);
    break;
  case 'C': case 'a': move_to(x + n, y); break;
  case 'D': move_to(x - n, y); break;
  case 'E': move_to(0, y + n); break;
  case 'F': move_to(0, y - n); break;
  case 'G': case '`': move_to(n - 1, y); break;
  case 'd': move_to(x, n - 1); break;
  case 'H': case 'f': move_to(param(1, 1) - 1, param(0, 1) - 1); break;
  case 'J': /* erase in display */
    switch (param(0, 0)) {
    case 0:
      clear_cells(y, x, COLS);
      for (int r = y + 1; r < ROWS; r++)
        clear_cells(r, 0, COLS);
      break;
    case 1:
      clear_cells(y, 0, x + 1);
      for (int r = 0; r < y; r++)
        clear_cells(r, 0, COLS);
      break;
    case 2: case 3:
      for (int r = 0; r < ROWS; r++)
        clear_cells(r, 0, COLS);
      break;
    }
    break;
  case 'K': /* erase in line */
    switch (param(0, 0)) {
    case 0: clear_cells(y, x, COLS); break;
    case 1: clear_cells(y, 0, x + 1); break;
    case 2: clear_cells(y, 0, COLS); break;
    }
    break;
  case 'L': /* insert lines */
    if (y >= top && y <= bottom)
      scroll(y, bottom, -n);
    break;
  case 'M': /* delete lines */
    if (y >= top && y <= bottom)
      scroll(y, bottom, n);
    break;
  case 'P': /* delete characters */
    if (n > COLS - x)
      n = COLS - x;
    memmove(&grid->cells[y][x], &grid->cells[y][x + n],
            (COLS - x - n) * sizeof(cell_t));
    clear_cells(y, COLS - n, COLS);
    break;
  case 'X': clear_cells(y, x, x + n); break;
  case 'S': scroll(top, bottom, n); break;
  case 'T': scroll(top, bottom, -n); break;
  case 'm': sgr(); break;
  case 'r': { /* scroll region */
    int t = param(0, 1) - 1, b = param(1, ROWS) - 1;
    if (b >= ROWS)
      b = ROWS - 1;
    if (t < b) {
      top = t, bottom = b;
      move_to(0, 0);
    }
    break;
  }
  case 's': save_cursor(); break;
  case 'u': restore_cursor(); break;
  case 'h': set_mode(true); break;
  case 'l': set_mode(false); break;
  case 'n': /* status report */
    if (param(0, 0) == 5) {
      reply("\033[0n");
    } else if (param(0, 0) == 6) {
      char buf[32];
      snprintf(buf, sizeof(buf), "\033[%d;%dR", y + 1, x + 1);
      reply(buf);
    }
    break;
  case 'c': /* device attributes: a VT102 */
    if (!private_mode)
      reply("\033[?6c");
    break;
  }
}

static void feed(unsigned char c) {
  /* Control characters work in the middle of sequences too. */
  if (c < 0x20 && state != OSC && state != OSC_ESC) {
    switch (c) {
    case '\a': return;
    case '\b':
      if (grid->x > 0)
        grid->x--;
      wrap_next = false;
      return;
    case '\t': {
      int x = (grid->x / 8 + 1) * 8;
      move_to(x >= COLS ? COLS - 1 : x, grid->y);
      return;
    }
    case '\n': case '\v': case '\f': linefeed(); return;
    case '\r': grid->x = 0; wrap_next = false; return;
    case 0x0E: charset = 1; return; /* SO */
    case 0x0F: charset = 0; return; /* SI */
    case 0x18: case 0x1A: state = GROUND; return; /* CAN, SUB */
    case 0x1B:
      state = ESC;
      return;
    default:
      return;
    }
  }

  switch (state) {
  case GROUND:
    if (utf8_left) {
      if ((c & 0xC0) == 0x80) {
        utf8_cp = utf8_cp << 6 | (c & 0x3F);
        if (--utf8_left == 0)
          put_char(from_unicode(utf8_cp));
        return;
      }
      utf8_left = 0;
      put_char('?'); /* broken sequence; c is handled below */
    }
    if (c < 0x80) {
      if (c != 0x7F)
        put_char(c);
    } else if ((c & 0xE0) == 0xC0) {
      utf8_cp = c & 0x1F, utf8_left = 1;
    } else if ((c & 0xF0) == 0xE0) {
      utf8_cp = c & 0x0F, utf8_left = 2;
    } else if ((c & 0xF8) == 0xF0) {
      utf8_cp = c & 0x07, utf8_left = 3;
    } else {
      put_char('?');
    }
    return;

  case ESC:
    state = GROUND;
    switch (c) {
    case '[':
      state = CSI;
      nparams = 0;
      params[0] = 0;
      private_mode = false;
      intermediate = 0;
      return;
    case ']': state = OSC; return;
    case '(': case ')': state = CHARSET; charset_slot = c; return;
    case '7': save_cursor(); return;
    case '8': restore_cursor(); return;
    case 'D': linefeed(); return;
    case 'E': grid->x = 0; linefeed(); return;
    case 'M': reverse_index(); return;
    case 'c': reset(); return;
    default: return; /* ESC = / ESC > (keypad modes) and others */
    }

  case CHARSET:
    charset_g[charset_slot == ')'] = c == '0';
    state = GROUND;
    return;

  case CSI:
    if (c >= '0' && c <= '9') {
      if (nparams == 0)
        nparams = 1;
      int *p = &params[nparams - 1];
      if (*p < 10000)
        *p = *p * 10 + (c - '0');
    } else if (c == ';' || c == ':') {
      if (nparams == 0)
        nparams = 1;
      if (nparams < MAX_PARAMS)
        params[nparams++] = 0;
    } else if (c == '?' || c == '>' || c == '=') {
      private_mode = true;
    } else if (c >= 0x20 && c <= 0x2F) {
      intermediate = c;
    } else if (c >= 0x40 && c <= 0x7E) {
      csi(c);
      state = GROUND;
    }
    return;

  case OSC: /* window titles: ignored, up to BEL or ESC \ */
    if (c == '\a')
      state = GROUND;
    else if (c == 0x1B)
      state = OSC_ESC;
    return;

  case OSC_ESC:
    state = c == '\\' ? GROUND : OSC;
    return;
  }
}

/* -- Drawing --------------------------------------------------------------- */

static window_t *win;
static int win_w, win_h;
static uint32_t palette(uint32_t c) {
  static const uint32_t base[16] = {
      0x1E1E1E, 0xCD3131, 0x0DBC79, 0xE5E510, 0x2472C8, 0xBC3FBC,
      0x11A8CD, 0xCCCCCC, 0x666666, 0xF14C4C, 0x23D18B, 0xF5F543,
      0x3B8EEA, 0xD670D6, 0x29B8DB, 0xFFFFFF,
  };
  uint32_t rgb;
  if (c & DIRECT) {
    rgb = c & 0xFFFFFF;
  } else if (c < 16) {
    rgb = base[c];
  } else if (c < 232) { /* 6x6x6 cube */
    static const uint8_t level[6] = {0, 95, 135, 175, 215, 255};
    c -= 16;
    rgb = level[c / 36] << 16 | level[c / 6 % 6] << 8 | level[c % 6];
  } else { /* grey ramp */
    uint32_t v = 8 + (c - 232) * 10;
    rgb = v << 16 | v << 8 | v;
  }
  return 0xFF000000 | rgb;
}

static void draw_cell(int cx, int cy, const cell_t *cell, bool cursor) {
  uint32_t fg = cell->fg, bg = cell->bg;
  if (cell->attr & ATTR_BOLD && fg < 8)
    fg += 8; /* bold is bright, as on the linux console */
  uint32_t fgc = palette(fg), bgc = palette(bg);
  if (!!(cell->attr & ATTR_REVERSE) != cursor) {
    uint32_t t = fgc;
    fgc = bgc, bgc = t;
  }

  uint32_t *buf = win->surface->buffer;
  int px = PAD + cx * CELL_W, py = PAD + cy * CELL_H;
  for (int r = 0; r < CELL_H; r++)
    for (int col = 0; col < CELL_W; col++)
      buf[(py + r) * win_w + px + col] = bgc;

  const uint8_t *glyph = get_glyph(cell->ch);
  if (glyph && cell->ch != ' ') {
    for (int r = 0; r < 8; r++) {
      uint8_t line = glyph[r];
      if (cell->attr & ATTR_BOLD)
        line |= line >> 1;
      for (int col = 0; col < 8; col++)
        if (line & (0x80 >> col))
          buf[(py + 2 + r) * win_w + px + col] = fgc;
    }
  }
  if (cell->attr & ATTR_UNDERLINE)
    for (int col = 0; col < CELL_W; col++)
      buf[(py + CELL_H - 1) * win_w + px + col] = fgc;
}

/* Draw the rows that changed and tell the server. */
static void render(void) {
  bool show = cursor_visible && !wrap_next;
  if (cursor_drawn_y != grid->y || !show)
    mark(cursor_drawn_y);
  if (show)
    mark(grid->y);

  int first = -1, last = -1;
  for (int y = 0; y < ROWS; y++) {
    if (!dirty[y])
      continue;
    for (int x = 0; x < COLS; x++)
      draw_cell(x, y, &grid->cells[y][x], show && y == grid->y && x == grid->x);
    dirty[y] = false;
    if (first < 0)
      first = y;
    last = y;
  }
  cursor_drawn_y = show ? grid->y : -1;

  if (first >= 0)
    object_damage(win->surface, 0, PAD + first * CELL_H, win_w,
                  (last - first + 1) * CELL_H);
}

/* -- Keys ------------------------------------------------------------------ */

static void on_key(window_t *w, const input_event_t *ev) {
  (void)w;
  char buf[8];
  const char *s = buf;

  if (ev->type == KEY_TYPE_CHAR) {
    char c = ev->character;
    int n = 0;
    if (ev->alt)
      buf[n++] = '\033';
    if (ev->ctrl) {
      if (c >= 'a' && c <= 'z')
        c -= 'a' - 1;
      else if (c >= '@' && c <= '_')
        c -= '@';
      else if (c == ' ' || c == '2')
        c = 0;
      else if (c == '/' || c == '-')
        c = 0x1F;
    }
    buf[n++] = c;
    buf[n] = '\0';
    write(master, buf, n); /* may be a NUL (Ctrl+Space): not reply() */
    return;
  }
  if (ev->type != KEY_TYPE_SPECIAL)
    return;

  switch (ev->action) {
  case KEY_ACTION_ENTER: s = "\r"; break;
  case KEY_ACTION_BACKSPACE: s = "\177"; break;
  case KEY_ACTION_TAB: s = ev->shift ? "\033[Z" : "\t"; break;
  case KEY_ACTION_ESC: s = "\033"; break;
  case KEY_ACTION_UP: s = app_cursor ? "\033OA" : "\033[A"; break;
  case KEY_ACTION_DOWN: s = app_cursor ? "\033OB" : "\033[B"; break;
  case KEY_ACTION_RIGHT: s = app_cursor ? "\033OC" : "\033[C"; break;
  case KEY_ACTION_LEFT: s = app_cursor ? "\033OD" : "\033[D"; break;
  case KEY_ACTION_HOME: s = "\033[1~"; break;
  case KEY_ACTION_INSERT: s = "\033[2~"; break;
  case KEY_ACTION_DELETE: s = "\033[3~"; break;
  case KEY_ACTION_END: s = "\033[4~"; break;
  case KEY_ACTION_PAGE_UP: s = "\033[5~"; break;
  case KEY_ACTION_PAGE_DOWN: s = "\033[6~"; break;
  default: return;
  }
  reply(s);
}

/* -- Main ------------------------------------------------------------------ */

/* Start the shell on a new pty; its pid, with our side in master. */
static pid_t start_shell(void) {
  master = posix_openpt(O_RDWR | O_NOCTTY | O_CLOEXEC);
  if (master < 0 || grantpt(master) < 0 || unlockpt(master) < 0) {
    perror("term: pty");
    return -1;
  }
  const char *slave = ptsname(master);
  struct winsize ws = {.ws_row = ROWS, .ws_col = COLS};
  ioctl(master, TIOCSWINSZ, &ws);

  pid_t pid = fork();
  if (pid != 0)
    return pid;

  /* The child: a session of its own, with the pty as its terminal. */
  setsid();
  int fd = open(slave, O_RDWR);
  if (fd < 0)
    _exit(127);
  ioctl(fd, TIOCSCTTY, 0);
  dup2(fd, 0);
  dup2(fd, 1);
  dup2(fd, 2);
  if (fd > 2)
    close(fd);
  setenv("TERM", "linux", 1);
  execl(SHELL, "-sh", (char *)NULL); /* "-": a login shell, reads profile */
  perror(SHELL);
  _exit(127);
}

static bool closed; /* the title bar's close button */

static void on_close(window_t *w) {
  (void)w;
  closed = true;
}

int main(void) {
  if (gui_connect() < 0) {
    perror("term: no display server (is /bin/desktop running?)");
    return 1;
  }

  win_w = COLS * CELL_W + 2 * PAD;
  win_h = ROWS * CELL_H + 2 * PAD;
  /* Each new terminal a little further down and right. */
  int off = (getpid() % 8) * 24;
  win = window_create(60 + off, 60 + off, win_w, win_h);
  if (!win) {
    perror("term: window");
    return 1;
  }
  win->on_key = on_key;
  win->on_close = on_close;
  window_set_title(win, "Terminal");
  uint32_t *buf = win->surface->buffer;
  for (int i = 0; i < win_w * win_h; i++)
    buf[i] = palette(DEFAULT_BG);

  pid_t shell = start_shell();
  if (shell < 0)
    return 1;

  reset();
  render();
  gui_flush();

  for (;;) {
    struct pollfd fds[2] = {{gui_fd(), POLLIN, 0}, {master, POLLIN, 0}};
    /* The timeout notices the shell exiting if the pty never says so. */
    if (poll(fds, 2, 500) < 0 && errno != EINTR)
      break;
    if (fds[0].revents && gui_dispatch() < 0)
      break; /* the desktop is gone */
    if (closed)
      break;
    if (fds[1].revents & POLLIN) {
      unsigned char data[4096];
      ssize_t n = read(master, data, sizeof(data));
      if (n <= 0 && errno != EINTR && errno != EAGAIN)
        break;
      for (ssize_t i = 0; i < n; i++)
        feed(data[i]);
    } else if (fds[1].revents & (POLLHUP | POLLERR)) {
      break;
    }
    render();
    gui_flush();
    if (waitpid(shell, NULL, WNOHANG) == shell)
      break;
  }

  window_destroy(win);
  gui_flush();
  kill(shell, SIGHUP);
  waitpid(shell, NULL, WNOHANG);
  return 0;
}
