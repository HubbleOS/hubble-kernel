/*
 * panel - the desktop's taskbar, as a libgui application.
 *
 * A strip along the bottom of the screen (a GUI_WINDOW_PANEL window, so
 * the display server keeps it above the others): Terminal opens a
 * /bin/term window, Kernel log shows or hides the kernel log, the clock
 * ticks on the right, and Log out ends the panel, and with it the
 * desktop session (/bin/desktop starts the panel and waits for it).
 */
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <libgui.h>

#define TERM_APP "/bin/term"

#define BAR_H 40
#define BAR_BG rgb(22, 26, 36)
#define BAR_EDGE rgb(64, 76, 104) /* the bar's top line */
#define BTN_Y 6
#define BTN_H 28

#define LOG_COLS 100 /* characters per log line */
#define LOG_ROWS 32
#define LOG_FONT 12 /* px, FONT_MONO */
#define LOG_BG rgb(16, 18, 24)
#define LOG_FG rgb(190, 198, 210)
#define LOG_REFRESH 2 /* seconds between rereads while it is open */

static bool logged_out;

static void style(element_t *el, color_t bg, color_t hover, color_t pressed,
                  color_t fg, int radius) {
  el->style_set->normal->background_color = bg;
  el->style_set->hover->background_color = hover;
  el->style_set->pressed->background_color = pressed;
  el->style_set->normal->border_radius = radius;
  el->text_color = fg;
  element_apply_style(el, NULL);
}

static element_t *bar_button(window_t *bar, int x, int w, const char *text,
                             void (*click)(void)) {
  element_t *b = button_create(x, BTN_Y, w, BTN_H, text);
  style(b, rgb(40, 46, 62), rgb(60, 68, 92), rgb(84, 96, 128),
        rgb(228, 232, 240), 5);
  b->on_click = click;
  window_addElement(bar, b);
  return b;
}

/* -- Kernel log window ---------------------------------------------------- */

static window_t *log_win;
static label_t *log_lines[LOG_ROWS];

/* Copy one log line, dropping ANSI colour sequences, cut at LOG_COLS. */
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
  dst[n] = '\0';
}

/* Show the last LOG_ROWS lines of /proc/kmsg. */
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
    /* Only what changed: a reread every few seconds stays cheap. */
    if (log_lines[i]->text && strcmp(log_lines[i]->text, text) == 0)
      continue;
    label_set_text(log_lines[i], text);
    element_redraw(log_lines[i]);
  }
}

static void log_close(window_t *win) {
  (void)win;
  window_destroy(log_win); /* frees the element list, not the elements */
  for (int i = 0; i < LOG_ROWS; i++)
    label_destroy(log_lines[i]);
  log_win = NULL;
}

static void log_toggle(void) {
  if (log_win) {
    log_close(log_win);
    return;
  }
  font_t *f = font_get(FONT_MONO, LOG_FONT);
  int line_h = font_line_height(f), line_w = LOG_COLS * font_advance(f, 'M');
  int w = line_w + 16, h = LOG_ROWS * line_h + 16;
  log_win = window_create(48, 56, w, h);
  if (!log_win)
    return;
  window_set_title(log_win, "Kernel log");
  log_win->on_close = log_close;
  log_win->surface->bg_color = LOG_BG;
  for (int i = 0; i < w * h; i++)
    log_win->surface->buffer[i] = LOG_BG;
  for (int i = 0; i < LOG_ROWS; i++) {
    log_lines[i] = label_create(4, 8 + i * line_h, line_w + 8, line_h, "");
    style(log_lines[i], LOG_BG, LOG_BG, LOG_BG, LOG_FG, 0);
    log_lines[i]->font_mono = true;
    log_lines[i]->font_size = LOG_FONT;
    log_lines[i]->text_align = TEXT_ALIGN_LEFT;
    window_addElement(log_win, log_lines[i]);
  }
  log_refresh();
}

/* -- Taskbar -------------------------------------------------------------- */

static label_t *clock_label;

static void on_logout(void) { logged_out = true; }

/* A new terminal window (/bin/term), and any that ended reaped. */
static void on_terminal(void) {
  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;
  if (fork() == 0) {
    execl(TERM_APP, TERM_APP, (char *)NULL);
    perror(TERM_APP);
    _exit(127);
  }
}

static void clock_update(void) {
  char text[32];
  time_t now = time(NULL);
  strftime(text, sizeof(text), "%a %d %b  %H:%M", localtime(&now));
  if (clock_label->text && strcmp(clock_label->text, text) == 0)
    return;
  label_set_text(clock_label, text);
  element_redraw(clock_label);
}

static bool build_bar(void) {
  int w = gui_screen_width();
  window_t *bar = window_create_flags(0, gui_screen_height() - BAR_H, w,
                                      BAR_H, GUI_WINDOW_PANEL);
  if (!bar)
    return false;
  bar->surface->bg_color = BAR_BG;
  for (int i = 0; i < w * BAR_H; i++)
    bar->surface->buffer[i] = i < w ? BAR_EDGE : BAR_BG;

  /* The name, as a badge where a start menu would be. */
  label_t *name = label_create(8, BTN_Y, 96, BTN_H, "HubbleOS");
  color_t accent = rgb(66, 133, 244);
  style(name, accent, accent, accent, rgb(255, 255, 255), 5);
  window_addElement(bar, name);

  bar_button(bar, 116, 96, "Terminal", on_terminal);
  bar_button(bar, 220, 104, "Kernel log", log_toggle);

  bar_button(bar, w - 8 - 88, 88, "Log out", on_logout);
  clock_label = label_create(w - 8 - 88 - 16 - 152, BTN_Y, 152, BTN_H, "");
  style(clock_label, BAR_BG, BAR_BG, BAR_BG, rgb(200, 206, 220), 0);
  window_addElement(bar, clock_label);
  clock_update();
  return true;
}

int main(void) {
  if (gui_connect() < 0) {
    perror("panel: no display server (is /bin/desktop running?)");
    return 1;
  }
  if (!build_bar()) {
    perror("panel: window");
    return 1;
  }
  gui_flush();

  time_t last_log = 0;
  while (!logged_out) {
    struct pollfd pfd = {.fd = gui_fd(), .events = POLLIN};
    /* Wake every second for the clock. */
    if (poll(&pfd, 1, 1000) < 0 && errno != EINTR)
      return 1;
    if (pfd.revents && gui_dispatch() < 0)
      return 1; /* the desktop is gone */
    clock_update();
    time_t now = time(NULL);
    if (log_win && now - last_log >= LOG_REFRESH) {
      log_refresh();
      last_log = now;
    }
    gui_flush();
  }
  return 0;
}
