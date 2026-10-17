/*
 * desktop - the HubbleOS display server.
 *
 * libgui's server half (libgui/server.h) composites the windows of
 * client applications, which connect over GUI_SOCKET and share their
 * pixels as memfds. This file is the platform under it: /dev/fb0 for
 * the screen, /dev/input/events for pointer and keys.
 *
 * It starts /bin/panel, the kernel log and controls, and ends with it
 * (its Quit button) or on Esc, giving the screen back to the console.
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
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include <libgui/server.h>

#define PANEL "/bin/panel"

static framebuffer_info_t fb_info;
static bool quit;

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
  display_key(&ev);
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
      display_pointer(in->x, in->y, in->left);
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

/* Start the panel; its pid, or -1. */
static pid_t start_panel(void) {
  pid_t pid = fork();
  if (pid == 0) {
    execl(PANEL, PANEL, (char *)NULL);
    perror(PANEL);
    _exit(127);
  }
  return pid;
}

int main(void) {
  if (open_framebuffer() < 0)
    return 1;
  int in_fd = open("/dev/input/events", O_RDONLY);
  if (in_fd < 0) {
    perror("open /dev/input/events");
    return 1;
  }
  if (display_init(&fb_info) < 0) {
    perror("desktop: " GUI_SOCKET);
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

  input_state_t in = {.x = fb_info.width / 2, .y = fb_info.height / 2};
  display_pointer(in.x, in.y, false);
  pid_t panel = start_panel();

  while (!quit) {
    /* Draw once per batch of events, not once per event. */
    display_render();

    struct pollfd fds[32];
    fds[0] = (struct pollfd){.fd = in_fd, .events = POLLIN};
    int n = 1 + display_poll_fds(fds + 1, 31);
    if (poll(fds, n, -1) < 0)
      continue;
    if (fds[0].revents)
      handle_events(in_fd, &in);
    display_handle(fds + 1, n - 1);

    /* The panel's Quit ends the session. */
    if (panel > 0 && waitpid(panel, NULL, WNOHANG) == panel)
      quit = true;
  }

  ioctl(0, KDSETMODE, KD_TEXT);
  if (have_tty) {
    tcflush(0, TCIFLUSH);
    tcsetattr(0, TCSANOW, &saved);
  }
  return 0;
}
