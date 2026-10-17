#define _GNU_SOURCE /* memfd_create */
#include "window.h"

#include <errno.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <core/input/keyboard.h>
#include <core/input/pointer.h>
#include <proto/protocol.h>

static int sock = -1;
static window_t *windows;
static uint32_t next_id = 1;
static pointer_t pointer;
static bool quitting;
static int screen_w, screen_h;

/* Bytes of a message that arrived in pieces. */
static uint8_t inbuf[16 * sizeof(struct gui_msg)];
static size_t inlen;

/* -- Talking to the server ------------------------------------------------ */

/* Send @p msg, and @p fd with it if >= 0. A failure means the server is
 * gone; gui_dispatch() notices that when the connection reads EOF. */
static void send_msg(const struct gui_msg *msg, int fd) {
  const uint8_t *p = (const uint8_t *)msg;
  size_t left = sizeof(*msg);
  union {
    struct cmsghdr h;
    char space[CMSG_SPACE(sizeof(int))];
  } ctl;

  while (left) {
    struct iovec iov = {(void *)p, left};
    struct msghdr mh = {.msg_iov = &iov, .msg_iovlen = 1};
    if (fd >= 0) { /* the descriptor travels with the first byte */
      memset(&ctl, 0, sizeof(ctl));
      mh.msg_control = ctl.space;
      mh.msg_controllen = sizeof(ctl.space);
      struct cmsghdr *c = CMSG_FIRSTHDR(&mh);
      c->cmsg_level = SOL_SOCKET;
      c->cmsg_type = SCM_RIGHTS;
      c->cmsg_len = CMSG_LEN(sizeof(int));
      memcpy(CMSG_DATA(c), &fd, sizeof(int));
    }
    ssize_t n = sendmsg(sock, &mh, MSG_NOSIGNAL);
    if (n < 0 && errno == EINTR)
      continue;
    if (n <= 0)
      return;
    p += n;
    left -= n;
    fd = -1;
  }
}

static void send_simple(uint32_t type, window_t *win) {
  struct gui_msg msg = {.type = type, .window = win->id};
  send_msg(&msg, -1);
}

int gui_connect(void) {
  if (sock >= 0)
    return 0;
  int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (fd < 0)
    return -1;
  struct sockaddr_un addr = {.sun_family = AF_UNIX};
  strncpy(addr.sun_path, GUI_SOCKET, sizeof(addr.sun_path) - 1);
  if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    int err = errno;
    close(fd);
    errno = err;
    return -1;
  }
  /* The server starts with the screen size. */
  struct gui_msg msg;
  size_t got = 0;
  while (got < sizeof(msg)) {
    ssize_t n = read(fd, (uint8_t *)&msg + got, sizeof(msg) - got);
    if (n < 0 && errno == EINTR)
      continue;
    if (n <= 0 || (got + n == sizeof(msg) && msg.type != GUI_SCREEN)) {
      close(fd);
      errno = n < 0 ? errno : EPROTO;
      return -1;
    }
    got += n;
  }
  screen_w = msg.w;
  screen_h = msg.h;
  sock = fd;
  return 0;
}

int gui_fd(void) { return sock; }
int gui_screen_width(void) { return screen_w; }
int gui_screen_height(void) { return screen_h; }

/* -- Windows -------------------------------------------------------------- */

/* The surface's elements changed something: remember it for gui_flush. */
static void window_damage(object_t *obj, int x, int y, int w, int h) {
  window_t *win = obj->host;
  if (!win->damaged) {
    win->dmg_x = x, win->dmg_y = y, win->dmg_w = w, win->dmg_h = h;
    win->damaged = true;
    return;
  }
  int x2 = win->dmg_x + win->dmg_w > x + w ? win->dmg_x + win->dmg_w : x + w;
  int y2 = win->dmg_y + win->dmg_h > y + h ? win->dmg_y + win->dmg_h : y + h;
  win->dmg_x = win->dmg_x < x ? win->dmg_x : x;
  win->dmg_y = win->dmg_y < y ? win->dmg_y : y;
  win->dmg_w = x2 - win->dmg_x;
  win->dmg_h = y2 - win->dmg_y;
}

static window_t *find_window(uint32_t id) {
  for (window_t *w = windows; w; w = w->next)
    if (w->id == id)
      return w;
  return NULL;
}

window_t *window_create(int x, int y, int w, int h) {
  if (sock < 0 || w <= 0 || h <= 0 || w > GUI_MAX_SIZE || h > GUI_MAX_SIZE)
    return NULL;

  window_t *win = calloc(1, sizeof(*win));
  if (!win)
    return NULL;
  win->size = (size_t)w * h * sizeof(uint32_t);

  /* The buffer the server will map too. */
  int fd = memfd_create("libgui-window", MFD_CLOEXEC);
  void *pixels = MAP_FAILED;
  if (fd >= 0 && ftruncate(fd, win->size) == 0)
    pixels = mmap(NULL, win->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (pixels != MAP_FAILED)
    win->surface = object_create_on(x, y, w, h, rgb(255, 255, 255), pixels);
  if (!win->surface) {
    if (pixels != MAP_FAILED)
      munmap(pixels, win->size);
    if (fd >= 0)
      close(fd);
    free(win);
    return NULL;
  }
  for (size_t i = 0; i < (size_t)w * h; i++)
    win->surface->buffer[i] = win->surface->bg_color;
  win->surface->on_damage = window_damage;
  win->surface->host = win;

  win->id = next_id++;
  win->next = windows;
  windows = win;

  struct gui_msg msg = {
      .type = GUI_CREATE, .window = win->id, .x = x, .y = y, .w = w, .h = h};
  send_msg(&msg, fd);
  close(fd); /* the mapping keeps the memory; the server has its own fd */

  object_damage(win->surface, 0, 0, w, h);
  return win;
}

void window_destroy(window_t *win) {
  if (!win)
    return;
  send_simple(GUI_DESTROY, win);

  for (window_t **link = &windows; *link; link = &(*link)->next) {
    if (*link == win) {
      *link = win->next;
      break;
    }
  }
  if (pointer.obj == win->surface)
    pointer = (pointer_t){0}; /* its elements are going away */
  munmap(win->surface->buffer, win->size);
  object_destroy(win->surface);
  free(win);
}

int window_addElement(window_t *win, element_t *el) {
  if (!win || !el)
    return -1;
  object_add_element(win->surface, el);
  return 0;
}

/* -- Events --------------------------------------------------------------- */

static void handle(const struct gui_msg *msg) {
  window_t *win = find_window(msg->window);
  if (!win)
    return; /* destroyed while the event was on its way */

  switch (msg->type) {
  case GUI_POINTER:
    if (pointer_update(&pointer, win->surface, msg->x, msg->y,
                       msg->buttons & GUI_BUTTON_LEFT))
      send_simple(GUI_MOVE, win);
    break;
  case GUI_LEAVE:
    if (pointer.obj == win->surface)
      pointer_leave(&pointer);
    break;
  case GUI_KEY:
    keyboard_dispatch(&msg->key);
    break;
  }
}

void gui_flush(void) {
  for (window_t *win = windows; win; win = win->next) {
    if (!win->damaged)
      continue;
    struct gui_msg msg = {.type = GUI_DAMAGE,
                          .window = win->id,
                          .x = win->dmg_x,
                          .y = win->dmg_y,
                          .w = win->dmg_w,
                          .h = win->dmg_h};
    send_msg(&msg, -1);
    win->damaged = false;
  }
}

int gui_dispatch(void) {
  ssize_t n = read(sock, inbuf + inlen, sizeof(inbuf) - inlen);
  if (n < 0 && (errno == EINTR || errno == EAGAIN))
    return 0;
  if (n <= 0)
    return -1; /* the server is gone */
  inlen += n;

  size_t done = 0;
  while (inlen - done >= sizeof(struct gui_msg)) {
    struct gui_msg msg;
    memcpy(&msg, inbuf + done, sizeof(msg));
    handle(&msg);
    done += sizeof(msg);
  }
  memmove(inbuf, inbuf + done, inlen - done);
  inlen -= done;

  gui_flush();
  return 0;
}

void gui_quit(void) { quitting = true; }

int gui_run(void) {
  gui_flush();
  while (!quitting) {
    struct pollfd pfd = {.fd = sock, .events = POLLIN};
    if (poll(&pfd, 1, -1) < 0 && errno != EINTR)
      return -1;
    if (gui_dispatch() < 0)
      return -1;
  }
  return 0;
}
