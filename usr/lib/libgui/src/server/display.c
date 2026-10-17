/**
 * @file display.c
 * @brief The display server (see display.h)
 *
 * Clients are untrusted: every size, rectangle and buffer they send is
 * checked before use, their buffers are mapped read-only, and a client
 * that stops reading its events is disconnected instead of blocking the
 * desktop.
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
  int client;
  uint32_t id;
  size_t map_len;
} surface_t;

static client_t clients[MAX_CLIENTS];
static surface_t surfaces[MAX_SURFACES];
static int listen_fd = -1;
static cursor_t *cursor;

static int pointer_x, pointer_y;
static bool pointer_left;
static surface_t *focus;  /* gets the keys */
static surface_t *hover;  /* last told about the pointer */
static surface_t *grab;   /* the button went down here: events stay here */
static surface_t *moving; /* follows the pointer (GUI_MOVE) */
static int move_dx, move_dy;

/* -- Surfaces --------------------------------------------------------- */

static surface_t *surface_of(object_t *obj) {
  for (int i = 0; i < MAX_SURFACES; i++)
    if (surfaces[i].obj == obj)
      return &surfaces[i];
  return NULL;
}

static surface_t *find_surface(int client, uint32_t id) {
  for (int i = 0; i < MAX_SURFACES; i++)
    if (surfaces[i].obj && surfaces[i].client == client &&
        surfaces[i].id == id)
      return &surfaces[i];
  return NULL;
}

/* Topmost window at (x, y). */
static surface_t *surface_at(int x, int y) {
  layer_t *l = &compositor.layers[LAYER_WINDOWS];
  for (int i = l->count - 1; i >= 0; i--) {
    object_t *o = l->objects[i];
    if (x >= o->x && x < o->x + o->width && y >= o->y &&
        y < o->y + o->height)
      return surface_of(o);
  }
  return NULL;
}

static void surface_free(surface_t *s) {
  compositor_remove(s->obj, LAYER_WINDOWS); /* repaints what it covered */
  munmap(s->obj->buffer, s->map_len);
  object_destroy(s->obj);
  s->obj = NULL;
  if (focus == s)
    focus = NULL;
  if (hover == s)
    hover = NULL;
  if (grab == s)
    grab = NULL;
  if (moving == s)
    moving = NULL;
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
      /* Keep at least a corner of it on screen. */
      int x = m->x < -m->w + 1 ? -m->w + 1 : m->x > fb_width - 1 ? fb_width - 1 : m->x;
      int y = m->y < 0 ? 0 : m->y > fb_height - 1 ? fb_height - 1 : m->y;
      object_t *obj = object_create_on(x, y, m->w, m->h, 0, pixels);
      if (obj) {
        *s = (surface_t){obj, c, m->window, len};
        compositor_add(obj, LAYER_WINDOWS);
        focus = s;
        ok = true;
      } else {
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
    if (pointer_left && grab == s) {
      moving = s;
      move_dx = pointer_x - s->obj->x;
      move_dy = pointer_y - s->obj->y;
    }
    return true;
  case GUI_DESTROY:
    surface_free(s);
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
  background_create(rgb(20, 20, 20));
  cursor = cursor_create(16, 16, rgb(0, 0, 0), rgb(255, 255, 255));

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
      compositor_move_object(moving->obj, x - move_dx, y - move_dy);
    else
      moving = NULL;
  }

  surface_t *under = surface_at(x, y);
  if (press) {
    grab = under;
    if (under) {
      focus = under;
      compositor_bring_to_front(under->obj, LAYER_WINDOWS);
    }
  }

  /* While the button is down, the window it went down in keeps getting
   * the pointer, so it sees the release. */
  surface_t *target = grab ? grab : under;
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
  if (release)
    grab = NULL;
  reap();
}

void display_key(const input_event_t *ev) {
  if (focus)
    client_send(focus, (struct gui_msg){.type = GUI_KEY, .key = *ev});
  reap();
}

void display_render(void) { compositor_render(); }
