/**
 * @file socket.c
 * @brief Unix-domain stream sockets (see socket.h)
 *
 * A connection is two byte streams (stream.h) sharing one wait queue:
 * dir[i] is written by side i and read by the other side. Files passed
 * with a message wait in their direction's list, tagged with the stream
 * position of the message's first byte; a read never runs past the next
 * such position, so files always arrive with the bytes sent with them.
 *
 * Listening sockets keep their pending connections (the accepting side's
 * socket, already connected) until accept() takes them. Names, listener
 * state and backlogs are guarded by one global lock: connecting is rare.
 *
 * Known limit: a socket sent over itself (or a cycle of them) is never
 * freed; Linux needs a garbage collector for that case.
 */
#include "socket.h"
#include "stream.h"
#include <hubble/errno.h>
#include <hubble/string.h>
#include <mm/kmalloc.h>
#include <smp/atomic.h>
#include <smp/scheduler.h>
#include <smp/spinlock.h>
#include <smp/waitqueue.h>

#define UNIX_BUF_SIZE (64 * 1024) /* per direction */
#define UNIX_BACKLOG_MAX 32

typedef struct rights_msg {
  uint64_t pos; /* stream position of the message's first byte */
  unix_rights_t rights;
  struct rights_msg *next;
} rights_msg_t;

typedef struct {
  stream_t stream;
  rights_msg_t *head, *tail; /* under stream.lock, in position order */
} unix_dir_t;

typedef struct {
  wait_queue_t wq; /* both directions, both sides */
  unix_dir_t dir[2];
  atomic_t refs; /* one per side */
} unix_conn_t;

typedef enum {
  UNIX_NEW,
  UNIX_LISTENING,
  UNIX_CONNECTED,
} unix_state_t;

typedef struct unix_sock {
  unix_state_t state;
  wait_queue_t wq; /* listening: woken when a connection arrives */

  /* Bound name (name_len 0: unbound), listed in `bound`. */
  char name[UNIX_NAME_MAX];
  size_t name_len;
  struct unix_sock *next_bound;

  /* Listening: connections waiting for accept(), oldest first. */
  file_t *backlog[UNIX_BACKLOG_MAX];
  int backlog_count, backlog_max;

  /* Connected: which side of which connection. */
  unix_conn_t *conn;
  int side;

  uint32_t owner_pid; /* who created it (a listener: who listens) */
  uint32_t peer_pid;  /* connected: the other end's, for SO_PEERCRED */
} unix_sock_t;

/* Guards the bound list and every socket's name, state (before it is
 * connected) and backlog. */
static spinlock_t names_lock = SPINLOCK_INIT("unix names");
static unix_sock_t *bound;

static const file_ops_t unix_ops;

static bool nonblock(file_t *file) { return file->flags & FILE_O_NONBLOCK; }

/* The direction this side reads from, and the one it writes to. */
static unix_dir_t *rx(unix_sock_t *s) { return &s->conn->dir[1 - s->side]; }
static unix_dir_t *tx(unix_sock_t *s) { return &s->conn->dir[s->side]; }

static void rights_put(unix_rights_t *r) {
  for (int i = 0; i < r->count; i++)
    file_put(r->files[i]);
  r->count = 0;
}

/* -- Connections ------------------------------------------ */

static unix_conn_t *conn_new(void) {
  unix_conn_t *c = kzalloc(sizeof(*c));
  if (!c)
    return NULL;
  waitqueue_init(&c->wq);
  if (stream_init(&c->dir[0].stream, UNIX_BUF_SIZE, &c->wq) < 0) {
    kfree(c);
    return NULL;
  }
  if (stream_init(&c->dir[1].stream, UNIX_BUF_SIZE, &c->wq) < 0) {
    stream_destroy(&c->dir[0].stream);
    kfree(c);
    return NULL;
  }
  atomic_set(&c->refs, 2);
  return c;
}

/* One side is done; the second frees the connection and closes the files
 * still in flight. */
static void conn_put(unix_conn_t *c) {
  if (atomic_dec(&c->refs) != 1)
    return;
  for (int d = 0; d < 2; d++) {
    for (rights_msg_t *m = c->dir[d].head, *next; m; m = next) {
      next = m->next;
      rights_put(&m->rights);
      kfree(m);
    }
    stream_destroy(&c->dir[d].stream);
  }
  kfree(c);
}

static void connect_sides(unix_conn_t *c, unix_sock_t *a, unix_sock_t *b) {
  a->peer_pid = b->owner_pid;
  b->peer_pid = a->owner_pid;
  a->conn = b->conn = c;
  a->side = 0;
  b->side = 1;
  a->state = b->state = UNIX_CONNECTED;
}

/* -- Names ------------------------------------------------ */

/* Caller holds names_lock. */
static unix_sock_t *find_bound(const char *name, size_t len) {
  for (unix_sock_t *s = bound; s; s = s->next_bound)
    if (s->name_len == len && memcmp(s->name, name, len) == 0)
      return s;
  return NULL;
}

long unix_bind(file_t *file, const char *name, size_t len) {
  unix_sock_t *s = file->priv;
  if (len == 0 || len > UNIX_NAME_MAX)
    return -EINVAL;

  long err = 0;
  spinlock_acquire(&names_lock);
  if (s->state != UNIX_NEW || s->name_len)
    err = -EINVAL;
  else if (find_bound(name, len))
    err = -EADDRINUSE;
  if (!err) {
    memcpy(s->name, name, len);
    s->name_len = len;
    s->next_bound = bound;
    bound = s;
  }
  spinlock_release(&names_lock);
  return err;
}

/* -- Listening and connecting ----------------------------- */

long unix_listen(file_t *file, int backlog) {
  unix_sock_t *s = file->priv;
  long err = 0;
  spinlock_acquire(&names_lock);
  if (s->state == UNIX_CONNECTED || !s->name_len) {
    err = -EINVAL; /* nothing to connect to it by */
  } else {
    s->backlog_max = backlog < 1                  ? 1
                     : backlog > UNIX_BACKLOG_MAX ? UNIX_BACKLOG_MAX
                                                  : backlog;
    s->state = UNIX_LISTENING;
    s->owner_pid = get_current_task()->id.pid;
  }
  spinlock_release(&names_lock);
  return err;
}

long unix_connect(file_t *file, const char *name, size_t len) {
  unix_sock_t *s = file->priv;
  if (s->state == UNIX_CONNECTED)
    return -EISCONN;
  if (s->state == UNIX_LISTENING)
    return -EINVAL;
  if (len == 0 || len > UNIX_NAME_MAX)
    return -EINVAL;

  /* The accepting side's socket, connected up front: accept() only has
   * to hand it out. */
  unix_conn_t *c = conn_new();
  file_t *peer = c ? unix_socket_create() : NULL;
  if (!peer) {
    if (c) {
      conn_put(c);
      conn_put(c);
    }
    return -ENOMEM;
  }
  unix_sock_t *p = peer->priv;

  long err = 0;
  spinlock_acquire(&names_lock);
  unix_sock_t *l = find_bound(name, len);
  if (!l || l->state != UNIX_LISTENING)
    err = -ECONNREFUSED;
  else if (l->backlog_count >= l->backlog_max)
    err = -EAGAIN;
  else if (s->state != UNIX_NEW)
    err = -EISCONN; /* connected meanwhile */
  if (!err) {
    /* The accepted side stands for the listener's owner. */
    p->owner_pid = l->owner_pid;
    connect_sides(c, s, p);
    l->backlog[l->backlog_count++] = peer;
    waitqueue_wake_all(&l->wq);
  }
  spinlock_release(&names_lock);

  if (err) {
    file_put(peer); /* still unconnected: frees just the socket */
    conn_put(c);
    conn_put(c);
  }
  return err;
}

static bool has_pending(void *arg) {
  unix_sock_t *s = arg;
  return s->backlog_count > 0;
}

long unix_accept(file_t *file, file_t **conn) {
  unix_sock_t *s = file->priv;
  for (;;) {
    long err = -EAGAIN;
    spinlock_acquire(&names_lock);
    if (s->state != UNIX_LISTENING) {
      err = -EINVAL;
    } else if (s->backlog_count) {
      *conn = s->backlog[0];
      s->backlog_count--;
      memmove(s->backlog, s->backlog + 1,
              s->backlog_count * sizeof(s->backlog[0]));
      err = 0;
    }
    spinlock_release(&names_lock);

    if (err != -EAGAIN || nonblock(file))
      return err;
    waitqueue_wait_event(&s->wq, has_pending, s, 0);
  }
}

/* -- Data ------------------------------------------------- */

long unix_send(file_t *file, const void *buf, size_t len,
               unix_rights_t *rights, bool nb) {
  unix_sock_t *s = file->priv;
  if (s->state != UNIX_CONNECTED)
    return -ENOTCONN;
  unix_dir_t *d = tx(s);
  if (!d->stream.writer_open)
    return -EPIPE; /* shut down for writing */

  rights_msg_t *msg = NULL;
  if (rights && rights->count) {
    if (!len)
      return -EINVAL; /* files travel with bytes */
    if (nb && !(stream_write_poll(&d->stream) & FILE_POLLOUT))
      return -EAGAIN;
    msg = kzalloc(sizeof(*msg));
    if (!msg)
      return -ENOMEM;
    msg->rights = *rights;

    spinlock_acquire(&d->stream.lock);
    if (!d->stream.reader_open) {
      spinlock_release(&d->stream.lock);
      kfree(msg);
      return -EPIPE;
    }
    msg->pos = d->stream.written;
    if (d->tail)
      d->tail->next = msg;
    else
      d->head = msg;
    d->tail = msg;
    spinlock_release(&d->stream.lock);
  }

  long n = stream_write(&d->stream, buf, len, nb);

  if (msg) {
    if (n > 0) {
      rights->count = 0; /* the references went with the message */
    } else {
      /* Nothing sent: take the message back; the caller keeps the
       * files. A reader can't have taken it, as it has no bytes. */
      spinlock_acquire(&d->stream.lock);
      rights_msg_t **link = &d->head, *prev = NULL;
      while (*link != msg) {
        prev = *link;
        link = &(*link)->next;
      }
      *link = msg->next;
      if (d->tail == msg)
        d->tail = prev;
      spinlock_release(&d->stream.lock);
      kfree(msg);
    }
  }
  return n;
}

long unix_recv(file_t *file, void *buf, size_t len, unix_rights_t *rights,
               bool nb, bool continuing) {
  unix_sock_t *s = file->priv;
  if (s->state != UNIX_CONNECTED)
    return -ENOTCONN;
  unix_dir_t *d = rx(s);
  if (!d->stream.reader_open || !len)
    return 0; /* shut down for reading */

  long err = stream_wait_readable(&d->stream, nb || continuing);
  if (err)
    return continuing ? 0 : err;

  /* Files sent with the next byte come with this read; the read stops
   * where the next files start. */
  rights_msg_t *msg = NULL;
  size_t limit = len;
  spinlock_acquire(&d->stream.lock);
  uint64_t pos = d->stream.written - d->stream.count;
  if (d->head && d->head->pos == pos) {
    if (continuing) {
      spinlock_release(&d->stream.lock);
      return 0;
    }
    msg = d->head;
    d->head = msg->next;
    if (!d->head)
      d->tail = NULL;
  }
  if (d->head && d->head->pos - pos < limit)
    limit = d->head->pos - pos; /* > 0: every message has bytes */
  spinlock_release(&d->stream.lock);

  if (msg) {
    if (rights)
      *rights = msg->rights;
    else
      rights_put(&msg->rights); /* read(2) has nowhere to put them */
    kfree(msg);
  }
  return stream_read(&d->stream, buf, limit, true);
}

long unix_peer_pid(file_t *file) {
  unix_sock_t *s = file->priv;
  return s->state == UNIX_CONNECTED ? (long)s->peer_pid : -ENOTCONN;
}

long unix_shutdown(file_t *file, bool read, bool write) {
  unix_sock_t *s = file->priv;
  if (s->state != UNIX_CONNECTED)
    return -ENOTCONN;
  if (read)
    stream_close(&rx(s)->stream, true);
  if (write)
    stream_close(&tx(s)->stream, false);
  return 0;
}

/* -- File operations -------------------------------------- */

static long unix_read(file_t *file, void *buf, size_t len) {
  return unix_recv(file, buf, len, NULL, nonblock(file), false);
}

static long unix_write(file_t *file, const void *buf, size_t len) {
  return unix_send(file, buf, len, NULL, nonblock(file));
}

static short unix_poll(file_t *file) {
  unix_sock_t *s = file->priv;
  switch (s->state) {
  case UNIX_LISTENING:
    return s->backlog_count ? FILE_POLLIN : 0;
  case UNIX_CONNECTED: {
    short events = stream_read_poll(&rx(s)->stream);
    /* A peer that stopped reading makes writes fail at once: that
     * counts as writable, as on Linux. */
    if (stream_write_poll(&tx(s)->stream) & (FILE_POLLOUT | FILE_POLLERR))
      events |= FILE_POLLOUT;
    return events;
  }
  default:
    return FILE_POLLHUP; /* not connected */
  }
}

static struct wait_queue *unix_wait_queue(file_t *file) {
  unix_sock_t *s = file->priv;
  return (struct wait_queue *)(s->state == UNIX_CONNECTED ? &s->conn->wq
                                                          : &s->wq);
}

static void unix_stat(file_t *file, uint32_t *type, uint64_t *size) {
  (void)file;
  *type = FILE_S_IFSOCK;
  *size = 0;
}

static void unix_release(file_t *file) {
  unix_sock_t *s = file->priv;

  /* Unname it and take back what never got accepted: those clients see
   * the connection close. */
  file_t *pending[UNIX_BACKLOG_MAX];
  int npending = 0;
  spinlock_acquire(&names_lock);
  for (unix_sock_t **link = &bound; *link; link = &(*link)->next_bound) {
    if (*link == s) {
      *link = s->next_bound;
      break;
    }
  }
  if (s->state == UNIX_LISTENING) {
    npending = s->backlog_count;
    memcpy(pending, s->backlog, npending * sizeof(pending[0]));
    s->backlog_count = 0;
  }
  spinlock_release(&names_lock);
  for (int i = 0; i < npending; i++)
    file_put(pending[i]);

  if (s->state == UNIX_CONNECTED) {
    /* The peer sees end of file, and EPIPE on writing. */
    stream_close(&rx(s)->stream, true);
    stream_close(&tx(s)->stream, false);
    conn_put(s->conn);
  }
  kfree(s);
}

static const file_ops_t unix_ops = {
    .read = unix_read,
    .write = unix_write,
    .poll = unix_poll,
    .wait_queue = unix_wait_queue,
    .stat = unix_stat,
    .release = unix_release,
};

/* -- Creation --------------------------------------------- */

file_t *unix_socket_create(void) {
  unix_sock_t *s = kzalloc(sizeof(*s));
  if (!s)
    return NULL;
  waitqueue_init(&s->wq);
  task_t *task = get_current_task();
  s->owner_pid = task ? task->id.pid : 0;
  file_t *file = file_alloc(&unix_ops, FILE_O_RDWR, s);
  if (!file)
    kfree(s);
  return file;
}

int unix_socketpair(file_t **a, file_t **b) {
  *a = unix_socket_create();
  *b = unix_socket_create();
  unix_conn_t *c = *a && *b ? conn_new() : NULL;
  if (!c) {
    if (*a)
      file_put(*a);
    if (*b)
      file_put(*b);
    return -ENOMEM;
  }
  connect_sides(c, (*a)->priv, (*b)->priv);
  return 0;
}

bool unix_is_socket(file_t *file) { return file->ops == &unix_ops; }
