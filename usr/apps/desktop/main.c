/*
 * desktop - the libgui desktop on HubbleOS.
 *
 * libgui is built in its GUI_DEMO configuration: it draws through the
 * framebuffer_info_t from platform/fb.h and takes keys from the queue in
 * platform/keyboard.c. This file is the HubbleOS side of that platform
 * layer (the SDL simulator was the other): /dev/fb0 for the screen,
 * /dev/input/events for mouse and keyboard.
 *
 * Esc quits and gives the screen back to the console.
 */
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <linux/kd.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <termios.h>
#include <unistd.h>

#include <libgui.h>
#include <dev/mouse/mouse.h>

#include "platform/fb.h"
#include "platform/keyboard.h"

#define LOG_COLS 70 /* characters per log line (8 px each) */
#define LOG_ROWS 24
#define LOG_LINE_H 12

static framebuffer_info_t fb_info;
static label_t *log_lines[LOG_ROWS];
static bool quit;

/* -- Kernel log window ---------------------------------------------------- */

/* Copy one log line, dropping ANSI colour sequences, padded with spaces
 * to LOG_COLS: labels centre their text, so full-width lines read as
 * left-aligned. */
static void log_line_text(char *dst, const char *src, size_t len) {
  size_t n = 0;
  for (size_t i = 0; i < len && n < LOG_COLS; i++) {
    if (src[i] == '\033') {
      while (i < len && !((src[i] >= 'A' && src[i] <= 'Z') ||
                          (src[i] >= 'a' && src[i] <= 'z')))
        i++;
      continue;
    }
    if (src[i] >= ' ' && src[i] < 127)
      dst[n++] = src[i];
  }
  memset(dst + n, ' ', LOG_COLS - n);
  dst[LOG_COLS] = '\0';
}

/* Show the last LOG_ROWS lines of /proc/kmsg. The old desktop showed the
 * console output from /pipe/tty0_out, which no longer exists. */
static void log_refresh(void) {
  static char buf[64 * 1024];
  size_t len = 0;
  int fd = open("/proc/kmsg", O_RDONLY);
  if (fd >= 0) {
    ssize_t n;
    /* Keep the tail when the log is larger than buf. */
    while ((n = read(fd, buf + len, sizeof(buf) - len)) > 0) {
      len += n;
      if (len == sizeof(buf)) {
        memmove(buf, buf + sizeof(buf) / 2, sizeof(buf) / 2);
        len = sizeof(buf) / 2;
      }
    }
    close(fd);
  }

  /* Walk back over the last LOG_ROWS lines. */
  size_t end = len, starts[LOG_ROWS];
  int rows = 0;
  while (end > 0 && buf[end - 1] == '\n')
    end--;
  size_t pos = end;
  while (rows < LOG_ROWS && pos > 0) {
    size_t s = pos;
    while (s > 0 && buf[s - 1] != '\n')
      s--;
    starts[rows++] = s;
    pos = s > 0 ? s - 1 : 0;
  }

  char text[LOG_COLS + 1];
  for (int i = 0; i < LOG_ROWS; i++) {
    int r = rows - 1 - i; /* oldest of the shown lines first */
    if (r >= 0) {
      size_t s = starts[r], e = s;
      while (e < end && buf[e] != '\n')
        e++;
      log_line_text(text, buf + s, e - s);
    } else {
      log_line_text(text, "", 0);
    }
    label_set_text(log_lines[i], text);
    element_redraw(log_lines[i]);
  }
}

static void build_log_window(void) {
  int w = LOG_COLS * 8 + 16, h = LOG_ROWS * LOG_LINE_H + 16;
  window_t *win = window_create(40, 40, w, h);
  for (int i = 0; i < LOG_ROWS; i++) {
    log_lines[i] = label_create(8, 8 + i * LOG_LINE_H, LOG_COLS * 8,
                                LOG_LINE_H, "");
    window_addElement(win, log_lines[i]);
  }
  log_refresh();
}

/* -- Controls window ------------------------------------------------------ */

static void on_quit(void) { quit = true; }

static void build_controls_window(void) {
  int x = fb_info.width - 300, y = 60;
  window_t *win = window_create(x, y, 260, 190);
  window_addElement(win, label_create(10, 10, 240, 20, "HubbleOS desktop"));
  window_addElement(win, input_create(10, 45, 240, 28, "type here"));
  window_addElement(win, button_create(10, 90, 240, 32, "Refresh log"));
  window_addElement(win, button_create(10, 135, 240, 32, "Quit"));
  /* libgui click handlers take no arguments, so tell them apart here. */
  element_t **els = win->surface->elements;
  els[2]->on_click = log_refresh;
  els[3]->on_click = on_quit;
}

/* -- Input ---------------------------------------------------------------- */

/* US layout, indexed by Linux key code: unshifted, shifted. */
static const char keymap[][2] = {
    [KEY_1] = {'1', '!'},  [KEY_2] = {'2', '@'},  [KEY_3] = {'3', '#'},
    [KEY_4] = {'4', '$'},  [KEY_5] = {'5', '%'},  [KEY_6] = {'6', '^'},
    [KEY_7] = {'7', '&'},  [KEY_8] = {'8', '*'},  [KEY_9] = {'9', '('},
    [KEY_0] = {'0', ')'},  [KEY_MINUS] = {'-', '_'}, [KEY_EQUAL] = {'=', '+'},
    [KEY_Q] = {'q', 'Q'},  [KEY_W] = {'w', 'W'},  [KEY_E] = {'e', 'E'},
    [KEY_R] = {'r', 'R'},  [KEY_T] = {'t', 'T'},  [KEY_Y] = {'y', 'Y'},
    [KEY_U] = {'u', 'U'},  [KEY_I] = {'i', 'I'},  [KEY_O] = {'o', 'O'},
    [KEY_P] = {'p', 'P'},  [KEY_LEFTBRACE] = {'[', '{'},
    [KEY_RIGHTBRACE] = {']', '}'}, [KEY_A] = {'a', 'A'}, [KEY_S] = {'s', 'S'},
    [KEY_D] = {'d', 'D'},  [KEY_F] = {'f', 'F'},  [KEY_G] = {'g', 'G'},
    [KEY_H] = {'h', 'H'},  [KEY_J] = {'j', 'J'},  [KEY_K] = {'k', 'K'},
    [KEY_L] = {'l', 'L'},  [KEY_SEMICOLON] = {';', ':'},
    [KEY_APOSTROPHE] = {'\'', '"'}, [KEY_GRAVE] = {'`', '~'},
    [KEY_BACKSLASH] = {'\\', '|'}, [KEY_Z] = {'z', 'Z'}, [KEY_X] = {'x', 'X'},
    [KEY_C] = {'c', 'C'},  [KEY_V] = {'v', 'V'},  [KEY_B] = {'b', 'B'},
    [KEY_N] = {'n', 'N'},  [KEY_M] = {'m', 'M'},  [KEY_COMMA] = {',', '<'},
    [KEY_DOT] = {'.', '>'}, [KEY_SLASH] = {'/', '?'}, [KEY_SPACE] = {' ', ' '},
};

typedef struct {
  int x, y;
  bool left;
  bool shift, ctrl, alt;
} input_state_t;

static void handle_key(input_state_t *in, int code, int value) {
  bool down = value != 0; /* 1 press, 2 autorepeat, 0 release */
  switch (code) {
  case KEY_LEFTSHIFT: case KEY_RIGHTSHIFT: in->shift = down; return;
  case KEY_LEFTCTRL: case KEY_RIGHTCTRL: in->ctrl = down; return;
  case KEY_LEFTALT: case KEY_RIGHTALT: in->alt = down; return;
  }
  if (!down)
    return;

  input_event_t ev = {.shift = in->shift, .ctrl = in->ctrl, .alt = in->alt};
  switch (code) {
  case KEY_ESC: quit = true; return;
  case KEY_BACKSPACE: ev.type = KEY_TYPE_SPECIAL; ev.action = KEY_ACTION_BACKSPACE; break;
  case KEY_ENTER: ev.type = KEY_TYPE_SPECIAL; ev.action = KEY_ACTION_ENTER; break;
  case KEY_TAB: ev.type = KEY_TYPE_SPECIAL; ev.action = KEY_ACTION_TAB; break;
  case KEY_UP: ev.type = KEY_TYPE_SPECIAL; ev.action = KEY_ACTION_UP; break;
  case KEY_DOWN: ev.type = KEY_TYPE_SPECIAL; ev.action = KEY_ACTION_DOWN; break;
  case KEY_LEFT: ev.type = KEY_TYPE_SPECIAL; ev.action = KEY_ACTION_LEFT; break;
  case KEY_RIGHT: ev.type = KEY_TYPE_SPECIAL; ev.action = KEY_ACTION_RIGHT; break;
  case KEY_DELETE: ev.type = KEY_TYPE_SPECIAL; ev.action = KEY_ACTION_DELETE; break;
  default:
    if (code < 0 || code >= (int)(sizeof(keymap) / sizeof(keymap[0])) ||
        !keymap[code][0])
      return;
    ev.type = KEY_TYPE_CHAR;
    ev.character = keymap[code][in->shift];
  }
  keyboard_queue_push(ev);
}

static cursor_t *cursor;

/* Apply one input frame (everything up to a SYN_REPORT) to libgui. */
static void gui_step(const input_state_t *in) {
  mouse_update(in->x, in->y, in->left);
  if (cursor->surface->x != in->x || cursor->surface->y != in->y)
    cursor_move(cursor, in->x, in->y);

  /* Dragging, as the old desktop did it: an element inside its window,
   * or the whole window when grabbed by its background. */
  if (g_mouse.drag_obj) {
    int dx = g_mouse.x - g_mouse.drag_offset_x;
    int dy = g_mouse.y - g_mouse.drag_offset_y;
    if (g_mouse.drag_el) {
      object_move_element(g_mouse.drag_obj, g_mouse.drag_el,
                          dx - g_mouse.drag_obj->x, dy - g_mouse.drag_obj->y);
    } else {
      compositor_move_object(g_mouse.drag_obj, dx, dy);
      compositor_bring_to_front(g_mouse.drag_obj, LAYER_WINDOWS);
    }
  }
  keyboard_update();
}

/* Read what is queued and apply it frame by frame. Applying a whole read
 * at once lost drags: when drawing is slow, the last moves and the button
 * release arrive together, and the release won before the moves. */
static void handle_events(int fd, input_state_t *in) {
  struct input_event ev[64];
  ssize_t n = read(fd, ev, sizeof(ev));
  for (ssize_t i = 0; i < n / (ssize_t)sizeof(ev[0]); i++) {
    if (ev[i].type == EV_REL && ev[i].code == REL_X)
      in->x += ev[i].value;
    else if (ev[i].type == EV_REL && ev[i].code == REL_Y)
      in->y += ev[i].value;
    else if (ev[i].type == EV_KEY && ev[i].code == BTN_LEFT)
      in->left = ev[i].value != 0;
    else if (ev[i].type == EV_KEY)
      handle_key(in, ev[i].code, ev[i].value);
    else if (ev[i].type == EV_SYN && ev[i].code == SYN_REPORT) {
      if (in->x < 0) in->x = 0;
      if (in->y < 0) in->y = 0;
      if (in->x >= (int)fb_info.width) in->x = fb_info.width - 1;
      if (in->y >= (int)fb_info.height) in->y = fb_info.height - 1;
      gui_step(in);
    }
  }
}

/* -- Main ----------------------------------------------------------------- */

static int open_framebuffer(void) {
  int fd = open("/dev/fb0", O_RDWR);
  if (fd < 0) {
    perror("open /dev/fb0");
    return -1;
  }
  struct fb_var_screeninfo var;
  struct fb_fix_screeninfo fix;
  if (ioctl(fd, FBIOGET_VSCREENINFO, &var) < 0 ||
      ioctl(fd, FBIOGET_FSCREENINFO, &fix) < 0) {
    perror("FBIOGET_*SCREENINFO");
    return -1;
  }
  /* libgui writes 32-bit XRGB pixels. */
  if (var.bits_per_pixel != 32 || var.red.offset != 16 ||
      var.green.offset != 8 || var.blue.offset != 0) {
    fprintf(stderr, "desktop: needs 32 bpp XRGB, got %u bpp\n",
            var.bits_per_pixel);
    return -1;
  }
  void *base = mmap(NULL, fix.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED,
                    fd, 0);
  if (base == MAP_FAILED) {
    perror("mmap /dev/fb0");
    return -1;
  }
  fb_info = (framebuffer_info_t){base, var.xres, var.yres, fix.line_length,
                                 32};
  return 0;
}

int main(void) {
  if (open_framebuffer() < 0)
    return 1;
  int in_fd = open("/dev/input/events", O_RDONLY);
  if (in_fd < 0) {
    perror("open /dev/input/events");
    return 1;
  }

  /* Keys reach the tty as well as us: don't echo them over the picture. */
  struct termios saved, quiet;
  bool have_tty = tcgetattr(0, &saved) == 0;
  if (have_tty) {
    quiet = saved;
    quiet.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(0, TCSANOW, &quiet);
  }
  ioctl(0, KDSETMODE, KD_GRAPHICS);

  screen_init(&fb_info);
  compositor_init();
  background_create(rgb(20, 20, 20));
  build_log_window();
  build_controls_window();
  cursor = cursor_create(16, 16, rgb(0, 0, 0), rgb(255, 255, 255));

  input_state_t in = {.x = fb_info.width / 2, .y = fb_info.height / 2};
  gui_step(&in);
  while (!quit) {
    /* Draw once per batch of input, not once per frame in it. */
    compositor_render();

    /* Sleep until input arrives instead of spinning. */
    struct pollfd pfd = {.fd = in_fd, .events = POLLIN};
    if (poll(&pfd, 1, -1) > 0)
      handle_events(in_fd, &in);
  }

  ioctl(0, KDSETMODE, KD_TEXT);
  if (have_tty) {
    tcflush(0, TCIFLUSH);
    tcsetattr(0, TCSANOW, &saved);
  }
  return 0;
}
