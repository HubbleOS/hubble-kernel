/**
 * @file display.c
 * @brief The display server (see display.h)
 *
 * Clients are untrusted: every size, rectangle and buffer they send is
 * checked before use, their buffers are mapped read-only, and a client
 * that stops reading its events is disconnected instead of blocking the
 * desktop.
 *
 * The server draws each ordinary window's frame: a shadow, a title bar
 * with the client's title and a close button, in an object of its own
 * just below the window's. Panels (GUI_WINDOW_PANEL) get no frame and
 * stay on top of the windows.
 */
#include "display.h"

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <proto/protocol.h>
#include <server/background/background.h>
#include <server/compositor/compositor.h>
#include <server/cursor/cursor.h>
#include <ui/text/font.h>

#define MAX_CLIENTS 16
#define MAX_SURFACES 64
#define MAX_PENDING_FDS 4 /* passed descriptors not yet used by a message */

typedef struct {
  int fd; /* -1: free slot */
  bool dead;

  uint8_t in[8 * sizeof(struct gui_msg)]; /* a message arriving in pieces */
  size_t inlen;
  int fds[MAX_PENDING_FDS];
  int nfds;

  uint8_t out[64 * sizeof(struct gui_msg)]; /* events it hasn't read */
  size_t outlen;
} client_t;

typedef struct {
  object_t *obj; /* NULL: free slot. Its buffer is the client's memory */
  object_t *frame; /* title bar, border and shadow; NULL for a panel */
  int client;
  uint32_t id;
  uint32_t flags; /* GUI_WINDOW_* */
  size_t map_len;
  char title[GUI_TITLE_MAX];
} surface_t;

/* Frame geometry: the border goes round the title bar and the window,
 * the shadow round all of it, a little lower. */
#define TITLE_H 26
#define BORDER 1
#define SHADOW 10
#define SHADOW_DY 3
#define CORNER 7       /* radius of the title bar's top corners */
#define CLOSE_R 7      /* close button */
#define TITLE_FONT 13  /* px */
#define FRAME_L (BORDER + SHADOW)
#define FRAME_T (TITLE_H + BORDER + SHADOW)

enum part { PART_NONE, PART_CONTENT, PART_TITLE, PART_CLOSE };

static client_t clients[MAX_CLIENTS];
static surface_t surfaces[MAX_SURFACES];
static int listen_fd = -1;
static cursor_t *cursor;

static int pointer_x, pointer_y;
static bool pointer_left;
static surface_t *focus;  /* gets the keys */
static surface_t *hover;  /* last told about the pointer */
static surface_t *grab;   /* the button went down here: events stay here */
static surface_t *moving; /* follows the pointer (GUI_MOVE, title bar) */
static int move_dx, move_dy;
static surface_t *closing; /* its close button is held down */
static bool chrome_press;  /* the button went down outside any window's
                            * content: no window gets the pointer */

/* -- Surfaces --------------------------------------------------------- */

static surface_t *surface_of(object_t *obj) {
  for (int i = 0; i < MAX_SURFACES; i++)
    if (surfaces[i].obj &&
        (surfaces[i].obj == obj || surfaces[i].frame == obj))
      return &surfaces[i];
  return NULL;
}

static int surface_layer(const surface_t *s) {
  return s->flags & GUI_WINDOW_PANEL ? LAYER_PANELS : LAYER_WINDOWS;
}

/* -- Frames ----------------------------------------------------------- */

/* Close button centre, in frame coordinates. */
static int close_cx(const object_t *f) { return f->width - FRAME_L - 16; }
#define CLOSE_CY (SHADOW + BORDER + TITLE_H / 2)

/* @p t / @p range of the way from @p a to @p b. */
static color_t mix(color_t a, color_t b, int t, int range) {
  int r = (int)get_red(a) + ((int)get_red(b) - (int)get_red(a)) * t / range;
  int g = (int)get_green(a) + ((int)get_green(b) - (int)get_green(a)) * t / range;
  int bl = (int)get_blue(a) + ((int)get_blue(b) - (int)get_blue(a)) * t / range;
  return rgb(r, g, bl);
}

/* @p text, shortened to "..." at the end until it fits in @p width
 * pixels, into @p out (strlen(text) + 4 bytes). */
static void fit_text(font_t *f, const char *text, int width, char *out) {
  size_t len = strlen(text);
  memcpy(out, text, len + 1);
  while (len && font_text_width(f, out) > width) {
    do /* back over one UTF-8 character */
      len--;
    while (len && (text[len] & 0xC0) == 0x80);
    memcpy(out + len, "...", 4);
  }
}

static void frame_draw(surface_t *s) {
  object_t *f = s->frame;
  if (!f)
    return;
  bool active = s == focus;
  color_t top = active ? rgb(62, 68, 88) : rgb(72, 76, 86);
  color_t bottom = active ? rgb(44, 49, 64) : rgb(60, 64, 74);
  color_t border = rgb(22, 24, 32);
  color_t text = active ? rgb(236, 239, 246) : rgb(160, 165, 175);
  color_t button = active ? rgb(232, 84, 72) : rgb(112, 116, 126);

  /* The framed rectangle, and the shadow's (lower). */
  int x0 = SHADOW, y0 = SHADOW, x1 = f->width - SHADOW, y1 = f->height - SHADOW;
  int title_y1 = y0 + BORDER + TITLE_H;
  int cx = close_cx(f);

  for (int y = 0; y < f->height; y++) {
    for (int x = 0; x < f->width; x++) {
      color_t c = 0;

      /* Shadow: fades out over SHADOW pixels. */
      int dx = x < x0 ? x0 - x : x >= x1 ? x - x1 + 1 : 0;
      int sy0 = y0 + SHADOW_DY, sy1 = y1 + SHADOW_DY;
      int dy = y < sy0 ? sy0 - y : y >= sy1 ? y - sy1 + 1 : 0;
      int d2 = dx * dx + dy * dy;
      if (d2 < SHADOW * SHADOW) {
        int t = SHADOW * SHADOW - d2; /* 0 .. SHADOW^2 */
        int a = 90 * t / (SHADOW * SHADOW) * t / (SHADOW * SHADOW);
        c = make_color(a, 0, 0, 0);
      }

      bool inside = x >= x0 && x < x1 && y >= y0 && y < y1;
      if (inside && y < y0 + CORNER) { /* round the top corners */
        int ccx = x < x0 + CORNER ? x0 + CORNER : x >= x1 - CORNER ? x1 - CORNER - 1 : x;
        int ex = x - ccx, ey = y - (y0 + CORNER);
        if (ex * ex + ey * ey > CORNER * CORNER)
          inside = false;
      }
      if (inside) {
        if (x < x0 + BORDER || x >= x1 - BORDER || y < y0 + BORDER ||
            y >= y1 - BORDER)
          c = border;
        else if (y < title_y1)
          c = mix(top, bottom, y - y0, TITLE_H);
        else
          c = 0; /* the window itself */

        /* Close button: a disc with a cross. */
        int bx = x - cx, by = y - CLOSE_CY;
        if (bx * bx + by * by <= CLOSE_R * CLOSE_R) {
          c = button;
          if ((bx == by || bx == -by) && bx >= -3 && bx <= 3)
            c = rgb(255, 255, 255);
        }
      }
      f->buffer[y * f->width + x] = c;
    }
  }

  /* The title, cut off before the close button. */
  font_t *font = font_get(FONT_SANS, TITLE_FONT);
  int baseline = CLOSE_CY - font_line_height(font) / 2 + font_ascent(font);
  int tx = x0 + BORDER + 12;
  char shown[GUI_TITLE_MAX + 3];
  fit_text(font, s->title, cx - CLOSE_R - 8 - tx, shown);
  font_draw(font, f->buffer, f->width, f->height, tx, baseline, shown, text);

  object_damage(f, 0, 0, f->width, f->height);
}

/* Which part of the frame (screen) @p x, @p y is in. */
static enum part frame_part(surface_t *s, int x, int y) {
  object_t *f = s->frame;
  int fx = x - f->x, fy = y - f->y;
  if (fx < SHADOW || fx >= f->width - SHADOW || fy < SHADOW ||
      fy >= f->height - SHADOW)
    return PART_NONE; /* the shadow is not part of the window */
  int bx = fx - close_cx(f), by = fy - CLOSE_CY;
  if (bx >= -CLOSE_R - 2 && bx <= CLOSE_R + 2 && by >= -CLOSE_R - 2 &&
      by <= CLOSE_R + 2)
    return PART_CLOSE;
  return PART_TITLE; /* the border drags the window too */
}

static void set_focus(surface_t *s) {
  surface_t *old = focus;
  focus = s;
  if (old == s)
    return;
  if (old)
    frame_draw(old);
  if (s)
    frame_draw(s);
}

static void surface_raise(surface_t *s) {
  if (s->frame)
    compositor_bring_to_front(s->frame, surface_layer(s));
  compositor_bring_to_front(s->obj, surface_layer(s));
}

/* Put the window's own pixels at @p x, @p y; its title bar stays on
 * screen. Panels stay where they are. */
static void surface_move(surface_t *s, int x, int y) {
  if (s->flags & GUI_WINDOW_PANEL)
    return;
  if (y < TITLE_H + BORDER)
    y = TITLE_H + BORDER;
  if (y > fb_height - 8)
    y = fb_height - 8;
  if (x < -s->obj->width + 40)
    x = -s->obj->width + 40;
  if (x > fb_width - 40)
    x = fb_width - 40;
  compositor_move_object(s->frame, x - FRAME_L, y - FRAME_T);
  compositor_move_object(s->obj, x, y);
}

static surface_t *find_surface(int client, uint32_t id) {
  for (int i = 0; i < MAX_SURFACES; i++)
    if (surfaces[i].obj && surfaces[i].client == client &&
        surfaces[i].id == id)
      return &surfaces[i];
  return NULL;
}

/* Topmost window at (x, y), and which part of it. */
static surface_t *surface_at(int x, int y, enum part *part) {
  static const int layers[] = {LAYER_PANELS, LAYER_WINDOWS};
  for (int k = 0; k < 2; k++) {
    layer_t *l = &compositor.layers[layers[k]];
    for (int i = l->count - 1; i >= 0; i--) {
      object_t *o = l->objects[i];
      if (x < o->x || x >= o->x + o->width || y < o->y ||
          y >= o->y + o->height)
        continue;
      surface_t *s = surface_of(o);
      if (!s)
        continue;
      *part = o == s->obj ? PART_CONTENT : frame_part(s, x, y);
      if (*part != PART_NONE)
        return s;
    }
  }
  *part = PART_NONE;
  return NULL;
}

/* The topmost framed window, to take the focus when the focused goes. */
static surface_t *top_window(void) {
  layer_t *l = &compositor.layers[LAYER_WINDOWS];
  for (int i = l->count - 1; i >= 0; i--) {
    surface_t *s = surface_of(l->objects[i]);
    if (s && s->frame)
      return s;
  }
  return NULL;
}

static void surface_free(surface_t *s) {
  compositor_remove(s->obj, surface_layer(s)); /* repaints what it covered */
  munmap(s->obj->buffer, s->map_len);
  object_destroy(s->obj);
  if (s->frame) {
    compositor_remove(s->frame, surface_layer(s));
    object_destroy(s->frame);
    s->frame = NULL;
  }
  s->obj = NULL;
  if (closing == s)
    closing = NULL;
  if (focus == s)
    set_focus(top_window());
  if (hover == s)
    hover = NULL;
  if (grab == s)
    grab = NULL;
  if (moving == s)
    moving = NULL;
}

/* The bottom of the screen above the panels along its bottom edge. */
static int work_bottom(void) {
  int bottom = fb_height;
  for (int i = 0; i < MAX_SURFACES; i++) {
    object_t *o = surfaces[i].obj;
    if (o && surfaces[i].flags & GUI_WINDOW_PANEL &&
        o->y + o->height == fb_height && o->y < bottom)
      bottom = o->y;
  }
  return bottom;
}

/* -- Clients ---------------------------------------------------------- */

static void client_drop(int c) {
  client_t *cl = &clients[c];
  for (int i = 0; i < MAX_SURFACES; i++)
    if (surfaces[i].obj && surfaces[i].client == c)
      surface_free(&surfaces[i]);
  for (int i = 0; i < cl->nfds; i++)
    close(cl->fds[i]);
  close(cl->fd);
  *cl = (client_t){.fd = -1};
}

/* Drop the clients marked dead: done between events, never midway. */
static void reap(void) {
  for (int c = 0; c < MAX_CLIENTS; c++)
    if (clients[c].fd >= 0 && clients[c].dead)
      client_drop(c);
}

static void client_write(client_t *cl) {
  while (cl->outlen && !cl->dead) {
    ssize_t n = send(cl->fd, cl->out, cl->outlen, MSG_DONTWAIT | MSG_NOSIGNAL);
    if (n < 0 && errno == EINTR)
      continue;
    if (n < 0 && errno == EAGAIN)
      return; /* poll() says when it can take more */
    if (n <= 0) {
      cl->dead = true;
      return;
    }
    memmove(cl->out, cl->out + n, cl->outlen - n);
    cl->outlen -= n;
  }
}

static void client_queue(client_t *cl, const struct gui_msg *msg) {
  if (cl->dead)
    return;
  if (cl->outlen + sizeof(*msg) > sizeof(cl->out)) {
    cl->dead = true; /* not reading its events: it would stall us */
    return;
  }
  memcpy(cl->out + cl->outlen, msg, sizeof(*msg));
  cl->outlen += sizeof(*msg);
  client_write(cl);
}

/* An event for the client that owns @p s. */
static void client_send(surface_t *s, struct gui_msg msg) {
  msg.window = s->id;
  client_queue(&clients[s->client], &msg);
}

static bool take_fd(client_t *cl, int *fd) {
  if (!cl->nfds)
    return false;
  *fd = cl->fds[0];
  memmove(cl->fds, cl->fds + 1, --cl->nfds * sizeof(int));
  return true;
}

/* -- Requests --------------------------------------------------------- */

static bool create_window(int c, const struct gui_msg *m) {
  client_t *cl = &clients[c];
  int fd;
  if (!take_fd(cl, &fd))
    return false;
  bool ok = false;
  size_t len = (size_t)m->w * m->h * sizeof(uint32_t);
  struct stat st;
  surface_t *s = NULL;
  for (int i = 0; i < MAX_SURFACES && !s; i++)
    if (!surfaces[i].obj)
      s = &surfaces[i];

  if (s && m->w > 0 && m->h > 0 && m->w <= GUI_MAX_SIZE &&
      m->h <= GUI_MAX_SIZE && !find_surface(c, m->window) &&
      fstat(fd, &st) == 0 && (uint64_t)st.st_size >= len) {
    /* Read-only: the server never writes a client's pixels. */
    void *pixels = mmap(NULL, len, PROT_READ, MAP_SHARED, fd, 0);
    if (pixels != MAP_FAILED) {
      uint32_t flags = m->flags & GUI_WINDOW_PANEL;
      /* Keep at least a corner of it, and its title bar, on screen. */
      int top = flags ? 0 : TITLE_H + BORDER;
      int x = m->x < -m->w + 1 ? -m->w + 1 : m->x > fb_width - 1 ? fb_width - 1 : m->x;
      int y = m->y < top ? top : m->y > fb_height - 1 ? fb_height - 1 : m->y;
      /* A window opens above the taskbar, as far as it fits. */
      if (!flags && y + m->h > work_bottom() - BORDER)
        y = work_bottom() - BORDER - m->h > top ? work_bottom() - BORDER - m->h
                                                : top;
      object_t *obj = object_create_on(x, y, m->w, m->h, 0, pixels);
      object_t *frame =
          flags ? NULL
                : object_create(x - FRAME_L, y - FRAME_T, m->w + 2 * FRAME_L,
                                m->h + TITLE_H + 2 * (BORDER + SHADOW), 0);
      if (obj && (flags || frame)) {
        *s = (surface_t){.obj = obj, .frame = frame, .client = c,
                         .id = m->window, .flags = flags, .map_len = len};
        if (frame)
          compositor_add(frame, LAYER_WINDOWS);
        compositor_add(obj, surface_layer(s));
        if (frame)
          set_focus(s); /* draws its frame, and the old focus's */
        ok = true;
      } else {
        if (obj)
          object_destroy(obj);
        munmap(pixels, len);
      }
    }
  }
  close(fd); /* the mapping keeps the memory */
  return ok;
}

static void damage_window(surface_t *s, const struct gui_msg *m) {
  /* 64-bit: the client's numbers may be anything. */
  int64_t x1 = m->x, y1 = m->y;
  int64_t x2 = x1 + m->w, y2 = y1 + m->h;
  if (x1 < 0) x1 = 0;
  if (y1 < 0) y1 = 0;
  if (x2 > s->obj->width) x2 = s->obj->width;
  if (y2 > s->obj->height) y2 = s->obj->height;
  if (x1 < x2 && y1 < y2)
    object_damage(s->obj, (int)x1, (int)y1, (int)(x2 - x1), (int)(y2 - y1));
}

/* One request. False: the client broke the protocol. */
static bool handle_msg(int c, const struct gui_msg *m) {
  if (m->type == GUI_CREATE)
    return create_window(c, m);

  surface_t *s = find_surface(c, m->window);
  if (!s)
    return m->type == GUI_DAMAGE || m->type == GUI_MOVE ||
           m->type == GUI_DESTROY; /* raced with its own destroy: fine */

  switch (m->type) {
  case GUI_DAMAGE:
    damage_window(s, m);
    return true;
  case GUI_MOVE:
    /* Only for the press that is still going on in this window. */
    if (pointer_left && grab == s && !(s->flags & GUI_WINDOW_PANEL)) {
      moving = s;
      move_dx = pointer_x - s->obj->x;
      move_dy = pointer_y - s->obj->y;
    }
    return true;
  case GUI_DESTROY:
    surface_free(s);
    return true;
  case GUI_TITLE:
    memcpy(s->title, m->title, sizeof(s->title));
    s->title[sizeof(s->title) - 1] = '\0';
    frame_draw(s);
    return true;
  default:
    return false;
  }
}

static void client_read(int c) {
  client_t *cl = &clients[c];
  struct iovec iov = {cl->in + cl->inlen, sizeof(cl->in) - cl->inlen};
  union {
    struct cmsghdr h;
    char space[CMSG_SPACE(sizeof(int) * MAX_PENDING_FDS)];
  } ctl;
  struct msghdr mh = {.msg_iov = &iov,
                      .msg_iovlen = 1,
                      .msg_control = ctl.space,
                      .msg_controllen = sizeof(ctl.space)};
  ssize_t n = recvmsg(cl->fd, &mh, MSG_DONTWAIT | MSG_CMSG_CLOEXEC);
  if (n < 0 && (errno == EAGAIN || errno == EINTR))
    return;
  if (n <= 0 || (mh.msg_flags & MSG_CTRUNC)) {
    cl->dead = true; /* gone, or sent more descriptors than allowed */
    return;
  }

  for (struct cmsghdr *h = CMSG_FIRSTHDR(&mh); h; h = CMSG_NXTHDR(&mh, h)) {
    if (h->cmsg_level != SOL_SOCKET || h->cmsg_type != SCM_RIGHTS)
      continue;
    int count = (h->cmsg_len - CMSG_LEN(0)) / sizeof(int);
    for (int i = 0; i < count; i++) {
      int fd;
      memcpy(&fd, CMSG_DATA(h) + i * sizeof(int), sizeof(int));
      if (cl->nfds < MAX_PENDING_FDS) {
        cl->fds[cl->nfds++] = fd;
      } else {
        close(fd);
        cl->dead = true;
      }
    }
  }

  cl->inlen += n;
  size_t done = 0;
  while (!cl->dead && cl->inlen - done >= sizeof(struct gui_msg)) {
    struct gui_msg m;
    memcpy(&m, cl->in + done, sizeof(m));
    if (!handle_msg(c, &m))
      cl->dead = true;
    done += sizeof(m);
  }
  memmove(cl->in, cl->in + done, cl->inlen - done);
  cl->inlen -= done;
}

static void accept_clients(void) {
  for (;;) {
    int fd = accept4(listen_fd, NULL, NULL, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (fd < 0)
      return;
    int c = 0;
    while (c < MAX_CLIENTS && clients[c].fd >= 0)
      c++;
    if (c == MAX_CLIENTS) {
      close(fd); /* full: the client sees the connection close */
      continue;
    }
    clients[c] = (client_t){.fd = fd};
    struct gui_msg screen = {.type = GUI_SCREEN, .w = fb_width, .h = fb_height};
    client_queue(&clients[c], &screen);
  }
}

/* -- Public API ------------------------------------------------------- */

int display_init(framebuffer_info_t *fb) {
  for (int c = 0; c < MAX_CLIENTS; c++)
    clients[c].fd = -1;

  screen_init(fb);
  compositor_init();
  background_create(rgb(46, 78, 120), rgb(14, 22, 38));
  cursor = cursor_create();

  listen_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
  if (listen_fd < 0)
    return -1;
  struct sockaddr_un addr = {.sun_family = AF_UNIX};
  strncpy(addr.sun_path, GUI_SOCKET, sizeof(addr.sun_path) - 1);
  unlink(GUI_SOCKET); /* on Linux a previous server leaves the file */
  if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0 ||
      listen(listen_fd, 8) < 0)
    return -1;
  return 0;
}

int display_poll_fds(struct pollfd *fds, int max) {
  int n = 0;
  if (n < max)
    fds[n++] = (struct pollfd){.fd = listen_fd, .events = POLLIN};
  for (int c = 0; c < MAX_CLIENTS && n < max; c++) {
    if (clients[c].fd < 0)
      continue;
    short events = POLLIN | (clients[c].outlen ? POLLOUT : 0);
    fds[n++] = (struct pollfd){.fd = clients[c].fd, .events = events};
  }
  return n;
}

void display_handle(const struct pollfd *fds, int n) {
  for (int i = 0; i < n; i++) {
    if (!fds[i].revents)
      continue;
    if (fds[i].fd == listen_fd) {
      accept_clients();
      continue;
    }
    for (int c = 0; c < MAX_CLIENTS; c++) {
      if (clients[c].fd != fds[i].fd)
        continue;
      if (fds[i].revents & POLLOUT)
        client_write(&clients[c]);
      if (fds[i].revents & (POLLIN | POLLHUP | POLLERR))
        client_read(c);
    }
  }
  reap();
}

void display_pointer(int x, int y, bool left) {
  bool press = left && !pointer_left, release = !left && pointer_left;
  pointer_x = x;
  pointer_y = y;
  pointer_left = left;

  if (cursor->surface->x != x || cursor->surface->y != y)
    cursor_move(cursor, x, y);

  if (moving) {
    if (left)
      surface_move(moving, x - move_dx, y - move_dy);
    else
      moving = NULL;
  }

  enum part part;
  surface_t *under = surface_at(x, y, &part);
  if (press) {
    grab = NULL;
    chrome_press = part != PART_CONTENT;
    if (under) {
      if (!(under->flags & GUI_WINDOW_PANEL))
        set_focus(under);
      surface_raise(under);
      if (part == PART_CONTENT) {
        grab = under;
      } else if (part == PART_CLOSE) {
        closing = under;
      } else {
        moving = under;
        move_dx = x - under->obj->x;
        move_dy = y - under->obj->y;
      }
    }
  }
  if (release && closing) {
    if (closing == under && part == PART_CLOSE)
      client_send(closing, (struct gui_msg){.type = GUI_CLOSE});
    closing = NULL;
  }

  /* While the button is down, the window it went down in keeps getting
   * the pointer, so it sees the release; one pressed on a title bar or
   * the desktop keeps it from every window. */
  surface_t *target = grab                         ? grab
                      : chrome_press && left       ? NULL
                      : part == PART_CONTENT       ? under
                                                   : NULL;
  if (hover != target) {
    if (hover)
      client_send(hover, (struct gui_msg){.type = GUI_LEAVE});
    hover = target;
  }
  if (target)
    client_send(target, (struct gui_msg){.type = GUI_POINTER,
                                         .x = x - target->obj->x,
                                         .y = y - target->obj->y,
                                         .buttons = left ? GUI_BUTTON_LEFT
                                                         : 0});
  if (release) {
    grab = NULL;
    chrome_press = false;
  }
  reap();
}

void display_key(const input_event_t *ev) {
  if (focus)
    client_send(focus, (struct gui_msg){.type = GUI_KEY, .key = *ev});
  reap();
}

void display_render(void) { compositor_render(); }
