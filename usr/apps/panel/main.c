/*
 * panel - the kernel log and a few controls, as a libgui application.
 *
 * Its windows are shown by the display server (/bin/desktop, which
 * starts it); Quit ends the panel, and with it the desktop session.
 */
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <libgui.h>

#define LOG_COLS 70 /* characters per log line (8 px each) */
#define LOG_ROWS 24
#define LOG_LINE_H 12

static label_t *log_lines[LOG_ROWS];

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

static void on_quit(void) { gui_quit(); }

static void build_controls_window(void) {
  int x = gui_screen_width() - 300, y = 60;
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

int main(void) {
  if (gui_connect() < 0) {
    perror("panel: no display server (is /bin/desktop running?)");
    return 1;
  }
  build_log_window();
  build_controls_window();
  return gui_run() < 0 ? 1 : 0;
}
