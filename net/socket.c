/*
 * Network sockets on lwIP's raw API (see socket.h, net.h).
 *
 *   SOCK_STREAM  TCP: received pbufs are chained in rx until read;
 *                tcp_recved() opens the window as the reader consumes.
 *   SOCK_DGRAM   UDP: a queue of datagrams with their senders.
 *   SOCK_RAW     copies of one IP protocol's packets (IPPROTO_ICMP for
 *                ping), IP header included, as on Linux. Sending adds
 *                the IP header (IP_HDRINCL, so IPPROTO_RAW, is not
 *                supported).
 *   AF_PACKET    Ethernet frames (SOCK_RAW) or their payloads
 *                (SOCK_DGRAM), from packet_tap(): what udhcpc uses before
 *                the interface has an address.
 *   AF_NETLINK   NETLINK_ROUTE (`ip`): a request is answered when it is
 *                sent (netlink.c), the replies queued as datagrams.
 *
 * Everything that touches lwIP or a socket's state runs under the net
 * lock, with interrupts off: user memory is only copied outside it,
 * through a kernel buffer. A syscall that has to wait drops the lock and
 * sleeps on the socket's wait queue; lwIP's callbacks wake it.
 */
#include "socket.h"
#include "net.h"

#include "lwip/ip4_addr.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/priv/tcp_priv.h"
#include "lwip/raw.h"
#include "lwip/tcp.h"
#include "lwip/udp.h"

#include <hpet/hpet.h>
#include <hubble/errno.h>
#include <hubble/string.h>
#include <mm/kmalloc.h>
#include <smp/scheduler.h>
#include <smp/task.h>
#include <smp/waitqueue.h>

/* -- Linux ABI ----------------------------------------------------------- */

#define SOCK_STREAM 1
#define SOCK_DGRAM 2
#define SOCK_RAW 3

#define IPPROTO_IP 0
#define IPPROTO_ICMP 1
#define IPPROTO_TCP 6
#define IPPROTO_UDP 17
#define IPPROTO_RAW 255 /* send with your own IP header (IP_HDRINCL) */

#define ETH_P_ALL 0x0003
#define ARPHRD_ETHER 1
#define PACKET_HOST 0
#define PACKET_BROADCAST 1
#define PACKET_MULTICAST 2
#define PACKET_OTHERHOST 3
#define PACKET_OUTGOING 4

#define SHUT_RD 0
#define SHUT_WR 1
#define SHUT_RDWR 2

#define SOL_IP 0
#define SOL_SOCKET 1
#define SOL_TCP 6
#define SOL_RAW 255
#define SOL_PACKET 263
#define SOL_NETLINK 270
#define NETLINK_ROUTE 0

#define SO_REUSEADDR 2
#define SO_TYPE 3
#define SO_ERROR 4
#define SO_DONTROUTE 5
#define SO_BROADCAST 6
#define SO_SNDBUF 7
#define SO_RCVBUF 8
#define SO_KEEPALIVE 9
#define SO_OOBINLINE 10
#define SO_LINGER 13
#define SO_REUSEPORT 15
#define SO_RCVLOWAT 18
#define SO_SNDLOWAT 19
#define SO_RCVTIMEO 20
#define SO_SNDTIMEO 21
#define SO_BINDTODEVICE 25
#define SO_ATTACH_FILTER 26
#define SO_DETACH_FILTER 27
#define SO_TIMESTAMP 29
#define SO_ACCEPTCONN 30
#define SO_PROTOCOL 38
#define SO_DOMAIN 39

#define IP_TOS 1
#define IP_TTL 2
#define TCP_NODELAY 1
#define TCP_MAXSEG 2

#define FIONREAD 0x541B
#define FIONBIO 0x5421

struct sockaddr_in {
  uint16_t sin_family;
  uint16_t sin_port; /* network order */
  uint32_t sin_addr; /* network order */
  uint8_t sin_zero[8];
};

struct sockaddr_ll {
  uint16_t sll_family;
  uint16_t sll_protocol; /* network order */
  int32_t sll_ifindex;
  uint16_t sll_hatype;
  uint8_t sll_pkttype;
  uint8_t sll_halen;
  uint8_t sll_addr[8];
};

struct timeval_k {
  long sec;
  long usec;
};

/* -- Sockets ------------------------------------------------------------- */

#define DGRAM_QUEUE 64        /* datagrams waiting to be read */
#define BACKLOG_MAX 32        /* connections waiting for accept() */
#define COPY_CHUNK (32 * 1024) /* bytes through the kernel buffer at once */

typedef enum {
  S_NEW,
  S_CONNECTING,
  S_CONNECTED,
  S_LISTENING,
  S_CLOSED, /* the connection is over (the pcb is gone) */
} sock_state_t;

typedef struct {
  struct pbuf *p;
  uint32_t addr; /* network order; AF_PACKET: unused */
  uint16_t port; /* host order */
  /* AF_PACKET */
  uint16_t proto; /* host order */
  int ifindex;
  uint8_t pkttype;
  uint8_t mac[6];
} dgram_t;

typedef struct net_sock {
  int domain, type, protocol;
  wait_queue_t wq;
  sock_state_t state;
  union {
    struct tcp_pcb *tcp;
    struct udp_pcb *udp;
    struct raw_pcb *raw;
  } pcb;

  int err; /* pending error (-errno), reported once */
  bool eof, shut_rd, shut_wr;
  bool can_send; /* TCP: room in the send buffer (for the waiters) */

  /* TCP: received, not read yet */
  struct pbuf *rx;
  /* listening: accepted connections, oldest first */
  struct net_sock *backlog[BACKLOG_MAX];
  int nbacklog, maxbacklog;

  /* datagrams */
  dgram_t q[DGRAM_QUEUE];
  int qhead, qcount;

  uint64_t rcvtimeo_ns, sndtimeo_ns;
  bool reuseaddr, keepalive, broadcast, nodelay;

  /* AF_PACKET */
  uint16_t pkt_proto; /* host order; 0: nothing until bound */
  int pkt_ifindex;    /* 0: every interface */

  /* AF_NETLINK */
  uint32_t nl_portid; /* our address; replies carry it */
  uint32_t nl_groups; /* multicast groups: accepted, nothing is sent */

  struct net_sock *next_listed; /* in `listed` */
} net_sock_t;

static const file_ops_t net_sock_ops;
/* The sockets lwIP keeps no list of that we need to walk: AF_PACKET (every
 * frame goes to them), raw IP (for /proc/net/raw) and AF_NETLINK (port ids
 * are unique). Under the net lock. */
static net_sock_t *listed;

bool net_is_socket(file_t *file) { return file && file->ops == &net_sock_ops; }

static net_sock_t *sock_of(file_t *file) { return file->priv; }

static long lwip_errno(err_t e) {
  switch (e) {
  case ERR_OK:
    return 0;
  case ERR_MEM:
    return -ENOMEM;
  case ERR_BUF:
    return -ENOBUFS;
  case ERR_TIMEOUT:
    return -ETIMEDOUT;
  case ERR_RTE:
    return -ENETUNREACH;
  case ERR_INPROGRESS:
    return -EINPROGRESS;
  case ERR_WOULDBLOCK:
    return -EAGAIN;
  case ERR_USE:
    return -EADDRINUSE;
  case ERR_ALREADY:
    return -EALREADY;
  case ERR_ISCONN:
    return -EISCONN;
  case ERR_CONN:
    return -ENOTCONN;
  case ERR_IF:
    return -ENETDOWN;
  case ERR_ABRT:
    return -ECONNABORTED;
  case ERR_RST:
    return -ECONNRESET;
  case ERR_CLSD:
    return -ENOTCONN;
  default:
    return -EINVAL;
  }
}

static net_sock_t *sock_new(int domain, int type, int protocol, bool atomic) {
  net_sock_t *s = kmalloc(sizeof(*s), (atomic ? KMALLOC_ATOMIC : KMALLOC_KERNEL) |
                                          KMALLOC_ZERO);
  if (!s)
    return NULL;
  s->domain = domain;
  s->type = type;
  s->protocol = protocol;
  waitqueue_init(&s->wq);
  return s;
}

static void wake(net_sock_t *s) { waitqueue_wake_all(&s->wq); }

static long take_err(net_sock_t *s) {
  long e = s->err;
  s->err = 0;
  return e;
}

/* Sleep until @p ready(s), the timeout or a signal. Called and returns
 * with the net lock held. */
static long sock_wait(net_sock_t *s, bool (*ready)(void *), bool nonblock,
                      uint64_t timeo_ns) {
  if (ready(s))
    return 0;
  if (nonblock)
    return -EAGAIN;
  uint64_t deadline = timeo_ns ? hpet_get_time_ns() + timeo_ns : 0;
  for (;;) {
    net_unlock();
    long r = waitqueue_wait_event(&s->wq, ready, s, deadline);
    net_lock();
    if (ready(s))
      return 0;
    if (r == -ETIMEDOUT)
      return -EAGAIN; /* SO_RCVTIMEO / SO_SNDTIMEO, as on Linux */
    if (r < 0)
      return r;
  }
}

/* -- Copying through a kernel buffer ------------------------------------- */

static size_t iov_total(const struct net_iov *iov, size_t n) {
  size_t t = 0;
  for (size_t i = 0; i < n; i++)
    t += iov[i].len;
  return t;
}

/* Copy @p n bytes from the iovecs, starting @p skip bytes in. */
static void iov_gather(const struct net_iov *iov, size_t niov, size_t skip,
                       uint8_t *dst, size_t n) {
  for (size_t i = 0; i < niov && n; i++) {
    if (skip >= iov[i].len) {
      skip -= iov[i].len;
      continue;
    }
    size_t k = iov[i].len - skip;
    if (k > n)
      k = n;
    memcpy(dst, (uint8_t *)iov[i].base + skip, k);
    dst += k;
    n -= k;
    skip = 0;
  }
}

static void iov_scatter(const struct net_iov *iov, size_t niov, size_t skip,
                        const uint8_t *src, size_t n) {
  for (size_t i = 0; i < niov && n; i++) {
    if (skip >= iov[i].len) {
      skip -= iov[i].len;
      continue;
    }
    size_t k = iov[i].len - skip;
    if (k > n)
      k = n;
    memcpy((uint8_t *)iov[i].base + skip, src, k);
    src += k;
    n -= k;
    skip = 0;
  }
}

/* -- Addresses ----------------------------------------------------------- */

static long get_sin(const void *addr, uint32_t len, uint32_t *ip,
                    uint16_t *port) {
  if (!addr)
    return -EFAULT;
  if (len < sizeof(struct sockaddr_in))
    return -EINVAL;
  const struct sockaddr_in *sin = addr;
  if (sin->sin_family != NET_AF_INET)
    return -EAFNOSUPPORT;
  *ip = sin->sin_addr;
  *port = lwip_ntohs(sin->sin_port);
  return 0;
}

static void put_sin(void *addr, uint32_t *len, uint32_t ip, uint16_t port) {
  if (!addr || !len)
    return;
  struct sockaddr_in sin = {0};
  sin.sin_family = NET_AF_INET;
  sin.sin_port = lwip_htons(port);
  sin.sin_addr = ip;
  uint32_t n = *len < sizeof(sin) ? *len : sizeof(sin);
  memcpy(addr, &sin, n);
  *len = sizeof(sin);
}

static void put_sll(void *addr, uint32_t *len, uint16_t proto, int ifindex,
                    uint8_t pkttype, const uint8_t mac[6]) {
  if (!addr || !len)
    return;
  struct sockaddr_ll sll = {0};
  sll.sll_family = NET_AF_PACKET;
  sll.sll_protocol = lwip_htons(proto);
  sll.sll_ifindex = ifindex;
  sll.sll_hatype = ARPHRD_ETHER;
  sll.sll_pkttype = pkttype;
  sll.sll_halen = 6;
  if (mac)
    memcpy(sll.sll_addr, mac, 6);
  uint32_t n = *len < sizeof(sll) ? *len : sizeof(sll);
  memcpy(addr, &sll, n);
  *len = sizeof(sll);
}

/* An address one of the interfaces has (or any, or 127/8). */
static bool local_addr(uint32_t ip) {
  if (ip == 0 || (lwip_ntohl(ip) >> 24) == 127)
    return true;
  struct netif *netif;
  NETIF_FOREACH(netif) {
    if (ip4_addr_get_u32(netif_ip4_addr(netif)) == ip)
      return true;
  }
  return false;
}

/* -- TCP callbacks (net lock held) --------------------------------------- */

static void tcp_update_room(net_sock_t *s) {
  struct tcp_pcb *pcb = s->pcb.tcp;
  s->can_send =
      pcb && tcp_sndbuf(pcb) > 0 && tcp_sndqueuelen(pcb) < TCP_SND_QUEUELEN;
}

static err_t tcp_recv_cb(void *arg, struct tcp_pcb *pcb, struct pbuf *p,
                         err_t err) {
  net_sock_t *s = arg;
  (void)err;
  if (!s) { /* closed by us: drop what still arrives */
    if (p) {
      tcp_recved(pcb, p->tot_len);
      pbuf_free(p);
    }
    return ERR_OK;
  }
  if (!p) {
    s->eof = true;
  } else if (s->shut_rd) {
    tcp_recved(pcb, p->tot_len);
    pbuf_free(p);
  } else if (s->rx) {
    pbuf_cat(s->rx, p);
  } else {
    s->rx = p;
  }
  wake(s);
  return ERR_OK;
}

static err_t tcp_sent_cb(void *arg, struct tcp_pcb *pcb, u16_t len) {
  net_sock_t *s = arg;
  (void)pcb, (void)len;
  if (s) {
    tcp_update_room(s);
    wake(s);
  }
  return ERR_OK;
}

/* The pcb is already freed by lwIP. */
static void tcp_err_cb(void *arg, err_t err) {
  net_sock_t *s = arg;
  if (!s)
    return;
  if (s->state == S_CONNECTING)
    s->err = err == ERR_RST ? -ECONNREFUSED : -ETIMEDOUT;
  else if (err != ERR_CLSD)
    s->err = lwip_errno(err);
  s->pcb.tcp = NULL;
  s->state = S_CLOSED;
  s->eof = true;
  s->can_send = false;
  wake(s);
}

static err_t tcp_connected_cb(void *arg, struct tcp_pcb *pcb, err_t err) {
  net_sock_t *s = arg;
  (void)pcb, (void)err;
  if (s) {
    s->state = S_CONNECTED;
    tcp_update_room(s);
    wake(s);
  }
  return ERR_OK;
}

static void tcp_setup(net_sock_t *s, struct tcp_pcb *pcb) {
  s->pcb.tcp = pcb;
  tcp_arg(pcb, s);
  tcp_recv(pcb, tcp_recv_cb);
  tcp_sent(pcb, tcp_sent_cb);
  tcp_err(pcb, tcp_err_cb);
}

static err_t tcp_accept_cb(void *arg, struct tcp_pcb *newpcb, err_t err) {
  net_sock_t *l = arg;
  if (err != ERR_OK || !newpcb)
    return ERR_VAL;
  net_sock_t *s = l ? sock_new(NET_AF_INET, SOCK_STREAM, IPPROTO_TCP, true)
                    : NULL;
  if (!s || l->nbacklog >= l->maxbacklog) {
    if (s)
      kfree(s);
    tcp_abort(newpcb);
    return ERR_ABRT;
  }
  tcp_setup(s, newpcb);
  s->state = S_CONNECTED;
  tcp_update_room(s);
  if (l->nodelay)
    tcp_nagle_disable(newpcb);
  l->backlog[l->nbacklog++] = s;
  wake(l);
  return ERR_OK;
}

/* Let go of a TCP connection: no more callbacks to @p s. */
static void tcp_drop(net_sock_t *s) {
  struct tcp_pcb *pcb = s->pcb.tcp;
  if (!pcb)
    return;
  s->pcb.tcp = NULL;
  tcp_arg(pcb, NULL);
  if (s->state == S_LISTENING) {
    tcp_close(pcb);
    return;
  }
  tcp_sent(pcb, NULL);
  tcp_err(pcb, NULL);
  /* Unread data: reset the connection, as Linux does. */
  if (s->rx || tcp_close(pcb) != ERR_OK)
    tcp_abort(pcb);
}

/* -- UDP and raw callbacks (net lock held) ------------------------------- */

static void queue_dgram(net_sock_t *s, const dgram_t *d) {
  if (s->qcount == DGRAM_QUEUE) {
    pbuf_free(d->p); /* full: dropped, as on Linux */
    return;
  }
  s->q[(s->qhead + s->qcount++) % DGRAM_QUEUE] = *d;
  wake(s);
}

static void udp_recv_cb(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                        const ip_addr_t *addr, u16_t port) {
  (void)pcb;
  dgram_t d = {.p = p, .addr = ip4_addr_get_u32(addr), .port = port};
  queue_dgram(arg, &d);
}

static u8_t raw_recv_cb(void *arg, struct raw_pcb *pcb, struct pbuf *p,
                        const ip_addr_t *addr) {
  (void)pcb;
  /* lwIP goes on with the packet (it answers pings itself), so keep a
   * copy, from the IP header on. */
  struct pbuf *copy = pbuf_clone(PBUF_RAW, PBUF_RAM, p);
  if (copy) {
    dgram_t d = {.p = copy, .addr = ip4_addr_get_u32(addr)};
    queue_dgram(arg, &d);
  }
  return 0; /* not eaten */
}

/* -- Packet sockets ------------------------------------------------------ */

void packet_tap(struct netif *netif, const uint8_t *frame, size_t len,
                bool outgoing) {
  if (!listed)
    return;
  uint16_t proto = (uint16_t)(frame[12] << 8 | frame[13]);
  int ifindex = netif_get_index(netif);
  struct netdev *dev = net_if_dev(netif);

  uint8_t pkttype = PACKET_OUTGOING;
  if (!outgoing) {
    static const uint8_t bcast[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    if (!memcmp(frame, bcast, 6))
      pkttype = PACKET_BROADCAST;
    else if (frame[0] & 1)
      pkttype = PACKET_MULTICAST;
    else if (dev && !memcmp(frame, dev->mac, 6))
      pkttype = PACKET_HOST;
    else
      pkttype = PACKET_OTHERHOST;
  }

  for (net_sock_t *s = listed; s; s = s->next_listed) {
    if (s->domain != NET_AF_PACKET || !s->pkt_proto || (s->pkt_ifindex && s->pkt_ifindex != ifindex))
      continue;
    if (s->pkt_proto != ETH_P_ALL && (outgoing || s->pkt_proto != proto))
      continue; /* outgoing frames only go to ETH_P_ALL sockets */
    size_t skip = s->type == SOCK_DGRAM ? 14 : 0;
    struct pbuf *p = pbuf_alloc(PBUF_RAW, (u16_t)(len - skip), PBUF_RAM);
    if (!p)
      continue;
    pbuf_take(p, frame + skip, (u16_t)(len - skip));
    dgram_t d = {.p = p, .proto = proto, .ifindex = ifindex,
                 .pkttype = pkttype};
    memcpy(d.mac, outgoing ? frame : frame + 6, 6);
    queue_dgram(s, &d);
  }
}

/* -- Creating and closing ------------------------------------------------ */

/* -- Netlink ------------------------------------------------------------- */

struct sockaddr_nl {
  uint16_t nl_family;
  uint16_t nl_pad;
  uint32_t nl_pid;
  uint32_t nl_groups;
};

static bool nl_portid_used(uint32_t id) {
  for (net_sock_t *s = listed; s; s = s->next_listed)
    if (s->domain == NET_AF_NETLINK && s->nl_portid == id)
      return true;
  return false;
}

/* As Linux's autobind: the process's pid if free, else counting down
 * from -4096. Net lock held. */
static uint32_t nl_new_portid(void) {
  static uint32_t next = (uint32_t)-4096;
  task_t *t = get_current_task();
  uint32_t id = t ? t->id.pid : 0;
  while (!id || nl_portid_used(id))
    id = next--;
  return id;
}

static void put_snl(void *addr, uint32_t *len, uint32_t portid,
                    uint32_t groups) {
  if (!addr || !len)
    return;
  struct sockaddr_nl snl = {
      .nl_family = NET_AF_NETLINK, .nl_pid = portid, .nl_groups = groups};
  memcpy(addr, &snl, *len < sizeof(snl) ? *len : sizeof(snl));
  *len = sizeof(snl);
}

/* A reply for a netlink socket's receive queue. Net lock held. */
static long nl_queue_reply(void *ctx, const void *data, size_t len) {
  net_sock_t *s = ctx;
  if (s->qcount == DGRAM_QUEUE)
    return -ENOBUFS; /* the reader is far behind, as on Linux */
  struct pbuf *p = pbuf_alloc(PBUF_RAW, (u16_t)len, PBUF_RAM);
  if (!p)
    return -ENOBUFS;
  pbuf_take(p, data, (u16_t)len);
  dgram_t d = {.p = p};
  queue_dgram(s, &d);
  return 0;
}

static long nl_bind(net_sock_t *s, const void *addr, uint32_t len) {
  if (!addr)
    return -EFAULT;
  if (len < sizeof(struct sockaddr_nl))
    return -EINVAL;
  const struct sockaddr_nl *snl = addr;
  if (snl->nl_family != NET_AF_NETLINK)
    return -EINVAL;
  long err = 0;
  net_lock();
  if (snl->nl_pid && snl->nl_pid != s->nl_portid) {
    if (nl_portid_used(snl->nl_pid))
      err = -EADDRINUSE;
    else
      s->nl_portid = snl->nl_pid;
  }
  if (!err)
    s->nl_groups = snl->nl_groups;
  net_unlock();
  return err;
}

static long sock_create(int domain, int type, int protocol, net_sock_t **out) {
  if (domain == NET_AF_INET) {
    if (type == SOCK_STREAM && (protocol == 0 || protocol == IPPROTO_TCP))
      protocol = IPPROTO_TCP;
    else if (type == SOCK_DGRAM && (protocol == 0 || protocol == IPPROTO_UDP))
      protocol = IPPROTO_UDP;
    else if (type == SOCK_RAW && protocol > 0 && protocol <= IPPROTO_RAW)
      ; /* copies of that protocol's packets; IPPROTO_RAW: ioctls only */
    else if (type == SOCK_STREAM || type == SOCK_DGRAM || type == SOCK_RAW)
      return -EPROTONOSUPPORT;
    else
      return -ESOCKTNOSUPPORT;
  } else if (domain == NET_AF_PACKET) {
    if (type != SOCK_RAW && type != SOCK_DGRAM)
      return -ESOCKTNOSUPPORT;
  } else if (domain == NET_AF_NETLINK) {
    if (type != SOCK_RAW && type != SOCK_DGRAM)
      return -ESOCKTNOSUPPORT;
    if (protocol != NETLINK_ROUTE)
      return -EPROTONOSUPPORT; /* no other netlink family */
  } else {
    return -EAFNOSUPPORT;
  }

  net_sock_t *s = sock_new(domain, type, protocol, false);
  if (!s)
    return -ENOMEM;

  long err = 0;
  net_lock();
  if (domain == NET_AF_PACKET) {
    s->pkt_proto = lwip_ntohs((uint16_t)protocol);
  } else if (domain == NET_AF_NETLINK) {
    s->nl_portid = nl_new_portid();
  } else if (type == SOCK_STREAM) {
    struct tcp_pcb *pcb = tcp_new_ip_type(IPADDR_TYPE_V4);
    if (pcb)
      tcp_setup(s, pcb);
    else
      err = -ENOBUFS;
  } else if (type == SOCK_DGRAM) {
    s->pcb.udp = udp_new_ip_type(IPADDR_TYPE_V4);
    if (s->pcb.udp)
      udp_recv(s->pcb.udp, udp_recv_cb, s);
    else
      err = -ENOBUFS;
  } else {
    s->pcb.raw = raw_new_ip_type(IPADDR_TYPE_V4, (u8_t)protocol);
    if (s->pcb.raw)
      raw_recv(s->pcb.raw, raw_recv_cb, s);
    else
      err = -ENOBUFS;
  }
  if (!err && (domain != NET_AF_INET || type == SOCK_RAW)) {
    s->next_listed = listed;
    listed = s;
  }
  net_unlock();

  if (err) {
    kfree(s);
    return err;
  }
  *out = s;
  return 0;
}

static void dgram_flush(net_sock_t *s) {
  while (s->qcount) {
    pbuf_free(s->q[s->qhead].p);
    s->qhead = (s->qhead + 1) % DGRAM_QUEUE;
    s->qcount--;
  }
}

/* Free @p s and everything it holds. Net lock held. */
static void sock_destroy(net_sock_t *s) {
  for (net_sock_t **pp = &listed; *pp; pp = &(*pp)->next_listed) {
    if (*pp == s) {
      *pp = s->next_listed;
      break;
    }
  }
  if (s->type == SOCK_STREAM) {
    tcp_drop(s);
    for (int i = 0; i < s->nbacklog; i++)
      sock_destroy(s->backlog[i]);
  } else if (s->type == SOCK_DGRAM && s->pcb.udp) {
    udp_remove(s->pcb.udp);
  } else if (s->type == SOCK_RAW && s->pcb.raw) {
    raw_remove(s->pcb.raw);
  }
  if (s->rx)
    pbuf_free(s->rx);
  dgram_flush(s);
  kfree(s);
}

long net_socket(int domain, int type, int protocol, file_t **out) {
  net_sock_t *s;
  long err = sock_create(domain, type, protocol, &s);
  if (err)
    return err;
  file_t *file = file_alloc(&net_sock_ops, FILE_O_RDWR, s);
  if (!file) {
    net_lock();
    sock_destroy(s);
    net_unlock();
    return -ENOMEM;
  }
  *out = file;
  return 0;
}

static void sock_release(file_t *file) {
  net_lock();
  sock_destroy(sock_of(file));
  net_unlock();
}

/* -- Naming and connecting ----------------------------------------------- */

long net_bind(file_t *file, const void *addr, uint32_t len) {
  net_sock_t *s = sock_of(file);
  if (s->domain == NET_AF_NETLINK)
    return nl_bind(s, addr, len);
  if (s->domain == NET_AF_PACKET) {
    if (!addr)
      return -EFAULT;
    if (len < 12)
      return -EINVAL;
    const struct sockaddr_ll *sll = addr;
    if (sll->sll_family != NET_AF_PACKET)
      return -EINVAL;
    net_lock();
    long err = 0;
    if (sll->sll_ifindex && !net_if_by_index(sll->sll_ifindex))
      err = -ENODEV;
    else {
      s->pkt_ifindex = sll->sll_ifindex;
      if (sll->sll_protocol)
        s->pkt_proto = lwip_ntohs(sll->sll_protocol);
    }
    net_unlock();
    return err;
  }

  uint32_t ip;
  uint16_t port;
  long err = get_sin(addr, len, &ip, &port);
  if (err)
    return err;
  ip_addr_t a;
  ip_addr_set_ip4_u32(&a, ip);

  net_lock();
  if (!local_addr(ip) && !(s->type != SOCK_STREAM && ip == 0xffffffff)) {
    err = -EADDRNOTAVAIL;
  } else if (s->type == SOCK_STREAM) {
    if (!s->pcb.tcp || s->state != S_NEW || s->pcb.tcp->local_port)
      err = -EINVAL;
    else {
      if (s->reuseaddr)
        ip_set_option(s->pcb.tcp, SOF_REUSEADDR);
      err = lwip_errno(tcp_bind(s->pcb.tcp, &a, port));
    }
  } else if (s->type == SOCK_DGRAM) {
    if (s->pcb.udp->local_port)
      err = -EINVAL;
    else {
      if (s->reuseaddr)
        ip_set_option(s->pcb.udp, SOF_REUSEADDR);
      err = lwip_errno(udp_bind(s->pcb.udp, &a, port));
    }
  } else {
    err = lwip_errno(raw_bind(s->pcb.raw, &a));
  }
  net_unlock();
  return err;
}

long net_listen(file_t *file, int backlog) {
  net_sock_t *s = sock_of(file);
  if (s->type != SOCK_STREAM)
    return -EOPNOTSUPP;
  if (backlog < 1)
    backlog = 1;
  if (backlog > BACKLOG_MAX)
    backlog = BACKLOG_MAX;

  long err = 0;
  net_lock();
  if (s->state == S_LISTENING) {
    s->maxbacklog = backlog;
  } else if (s->state != S_NEW || !s->pcb.tcp) {
    err = -EINVAL;
  } else {
    if (!s->pcb.tcp->local_port) /* not bound: any port, as on Linux */
      err = lwip_errno(tcp_bind(s->pcb.tcp, IP4_ADDR_ANY, 0));
    struct tcp_pcb *lpcb =
        err ? NULL : tcp_listen_with_backlog(s->pcb.tcp, (u8_t)backlog);
    if (!err && !lpcb) {
      err = -ENOBUFS;
    } else if (!err) {
      /* tcp_listen freed the old pcb. */
      s->pcb.tcp = lpcb;
      s->state = S_LISTENING;
      s->maxbacklog = backlog;
      tcp_arg(lpcb, s);
      tcp_accept(lpcb, tcp_accept_cb);
    }
  }
  net_unlock();
  return err;
}

static bool accept_ready(void *arg) {
  net_sock_t *s = arg;
  return s->nbacklog > 0 || s->state != S_LISTENING;
}

long net_accept(file_t *file, file_t **conn, void *addr, uint32_t *len) {
  net_sock_t *s = sock_of(file);
  if (s->type != SOCK_STREAM)
    return -EOPNOTSUPP;
  net_lock();
  long err = s->state == S_LISTENING ? 0 : -EINVAL;
  if (!err)
    err = sock_wait(s, accept_ready, file->flags & FILE_O_NONBLOCK,
                    s->rcvtimeo_ns);
  if (!err && s->state != S_LISTENING)
    err = -EINVAL;
  net_sock_t *c = NULL;
  uint32_t ip = 0;
  uint16_t port = 0;
  if (!err) {
    c = s->backlog[0];
    memmove(s->backlog, s->backlog + 1, --s->nbacklog * sizeof(c));
    if (c->pcb.tcp) {
      ip = ip4_addr_get_u32(ip_2_ip4(&c->pcb.tcp->remote_ip));
      port = c->pcb.tcp->remote_port;
      tcp_backlog_accepted(c->pcb.tcp);
    }
  }
  net_unlock();

  if (err)
    return err;
  file_t *newfile = file_alloc(&net_sock_ops, FILE_O_RDWR, c);
  if (!newfile) {
    net_lock();
    sock_destroy(c);
    net_unlock();
    return -ENOMEM;
  }
  put_sin(addr, len, ip, port);
  *conn = newfile;
  return 0;
}

static bool connect_ready(void *arg) {
  return ((net_sock_t *)arg)->state != S_CONNECTING;
}

long net_connect(file_t *file, const void *addr, uint32_t len) {
  net_sock_t *s = sock_of(file);
  if (s->domain == NET_AF_PACKET)
    return -EOPNOTSUPP;
  if (!addr)
    return -EFAULT;
  if (s->domain == NET_AF_NETLINK) {
    /* Only the kernel (port 0) answers; it is the default peer anyway. */
    const struct sockaddr_nl *snl = addr;
    if (len < sizeof(*snl) || snl->nl_family != NET_AF_NETLINK)
      return -EINVAL;
    return snl->nl_pid ? -ECONNREFUSED : 0;
  }

  /* AF_UNSPEC: a datagram socket forgets its peer. */
  if (len >= 2 && *(const uint16_t *)addr == 0 && s->type != SOCK_STREAM) {
    net_lock();
    if (s->type == SOCK_DGRAM)
      udp_disconnect(s->pcb.udp);
    else
      raw_disconnect(s->pcb.raw);
    net_unlock();
    return 0;
  }

  uint32_t ip;
  uint16_t port;
  long err = get_sin(addr, len, &ip, &port);
  if (err)
    return err;
  ip_addr_t a;
  ip_addr_set_ip4_u32(&a, ip);

  net_lock();
  if (s->type == SOCK_DGRAM) {
    err = lwip_errno(udp_connect(s->pcb.udp, &a, port));
  } else if (s->type == SOCK_RAW) {
    err = lwip_errno(raw_connect(s->pcb.raw, &a));
  } else if (s->state == S_CONNECTING) {
    err = -EALREADY;
  } else if (s->state == S_CONNECTED) {
    err = -EISCONN;
  } else if (s->state != S_NEW || !s->pcb.tcp) {
    err = s->err ? take_err(s) : -EINVAL;
  } else {
    if (s->keepalive)
      ip_set_option(s->pcb.tcp, SOF_KEEPALIVE);
    if (s->nodelay)
      tcp_nagle_disable(s->pcb.tcp);
    err = lwip_errno(tcp_connect(s->pcb.tcp, &a, port, tcp_connected_cb));
    if (!err) {
      s->state = S_CONNECTING;
      if (file->flags & FILE_O_NONBLOCK) {
        err = -EINPROGRESS;
      } else {
        err = sock_wait(s, connect_ready, false, s->sndtimeo_ns);
        if (err == -EAGAIN)
          err = -EINPROGRESS; /* SO_SNDTIMEO ran out; it goes on */
        else if (!err && s->state != S_CONNECTED)
          err = s->err ? take_err(s) : -ECONNREFUSED;
      }
    }
  }
  net_unlock();
  return err;
}

long net_shutdown(file_t *file, int how) {
  net_sock_t *s = sock_of(file);
  if (how != SHUT_RD && how != SHUT_WR && how != SHUT_RDWR)
    return -EINVAL;
  long err = 0;
  net_lock();
  if (s->type == SOCK_STREAM && s->state != S_CONNECTED &&
      s->state != S_CLOSED) {
    err = -ENOTCONN;
  } else {
    if (how != SHUT_WR) {
      s->shut_rd = true;
      if (s->rx && s->pcb.tcp)
        tcp_recved(s->pcb.tcp, s->rx->tot_len);
      if (s->rx)
        pbuf_free(s->rx);
      s->rx = NULL;
    }
    if (how != SHUT_RD) {
      s->shut_wr = true;
      if (s->type == SOCK_STREAM && s->pcb.tcp)
        tcp_shutdown(s->pcb.tcp, 0, 1);
    }
    wake(s);
  }
  net_unlock();
  return err;
}

/* -- Sending ------------------------------------------------------------- */

static bool send_ready(void *arg) {
  net_sock_t *s = arg;
  return s->can_send || s->err || !s->pcb.tcp || s->shut_wr;
}

/* Queue @p len bytes on a TCP connection. Net lock held. */
static long tcp_send_chunk(net_sock_t *s, const uint8_t *buf, size_t len,
                           bool nonblock, bool more) {
  size_t done = 0;
  while (done < len) {
    long err = sock_wait(s, send_ready, nonblock, s->sndtimeo_ns);
    if (err)
      return done ? (long)done : err;
    if (s->err)
      return done ? (long)done : take_err(s);
    if (!s->pcb.tcp || s->shut_wr)
      return done ? (long)done : -EPIPE;

    size_t n = len - done, room = tcp_sndbuf(s->pcb.tcp);
    if (n > room)
      n = room;
    err_t e = tcp_write(s->pcb.tcp, buf + done, (u16_t)n,
                        TCP_WRITE_FLAG_COPY |
                            (more || done + n < len ? TCP_WRITE_FLAG_MORE : 0));
    if (e == ERR_MEM) { /* the segment queue is full: wait for acks */
      tcp_output(s->pcb.tcp);
      s->can_send = false;
      continue;
    }
    if (e != ERR_OK)
      return done ? (long)done : lwip_errno(e);
    done += n;
    tcp_update_room(s);
  }
  if (!more)
    tcp_output(s->pcb.tcp);
  return (long)done;
}

static long dgram_send(net_sock_t *s, const uint8_t *buf, size_t len,
                       const void *addr, uint32_t addrlen) {
  long err = 0;
  uint32_t ip = 0;
  uint16_t port = 0;
  if (addr && s->domain == NET_AF_INET) {
    err = get_sin(addr, addrlen, &ip, &port);
    if (err)
      return err;
  }

  if (s->domain == NET_AF_NETLINK) {
    const struct sockaddr_nl *snl = addr;
    if (addr && (addrlen < sizeof(*snl) || snl->nl_family != NET_AF_NETLINK))
      return -EINVAL;
    if (snl && snl->nl_pid)
      return -ECONNREFUSED; /* to another socket: not supported */
    err = net_rtnl_request(buf, len, s->nl_portid, nl_queue_reply, s);
    return err ? err : (long)len;
  }

  if (s->domain == NET_AF_PACKET) {
    const struct sockaddr_ll *sll = addr;
    if (addr && addrlen < 12)
      return -EINVAL;
    int ifindex = sll ? sll->sll_ifindex : s->pkt_ifindex;
    struct netif *netif = net_if_by_index(ifindex);
    if (!netif)
      return -ENXIO;
    if (s->type == SOCK_RAW)
      return net_if_xmit(netif, buf, len) ?: (long)len;
    /* SOCK_DGRAM: we add the Ethernet header. */
    struct netdev *dev = net_if_dev(netif);
    if (!sll || !dev)
      return -EINVAL;
    if (len + 14 > NETDEV_FRAME_MAX)
      return -EMSGSIZE;
    static uint8_t frame[NETDEV_FRAME_MAX]; /* under the net lock */
    uint16_t proto = sll->sll_protocol ? sll->sll_protocol
                                       : lwip_htons(s->pkt_proto);
    memcpy(frame, sll->sll_addr, 6);
    memcpy(frame + 6, dev->mac, 6);
    memcpy(frame + 12, &proto, 2);
    memcpy(frame + 14, buf, len);
    return net_if_xmit(netif, frame, len + 14) ?: (long)len;
  }

  if (len > 0xffff - 28)
    return -EMSGSIZE;
  struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, (u16_t)len, PBUF_RAM);
  if (!p)
    return -ENOBUFS;
  pbuf_take(p, buf, (u16_t)len);

  ip_addr_t a;
  ip_addr_set_ip4_u32(&a, ip);
  err_t e;
  if (s->type == SOCK_DGRAM) {
    bool connected = s->pcb.udp->flags & UDP_FLAGS_CONNECTED;
    if (!addr && !connected)
      e = ERR_CONN;
    else if (addr)
      e = udp_sendto(s->pcb.udp, p, &a, port);
    else
      e = udp_send(s->pcb.udp, p);
  } else if (s->protocol == IPPROTO_RAW) {
    pbuf_free(p);
    return -EOPNOTSUPP;
  } else {
    bool connected = s->pcb.raw->flags & RAW_FLAGS_CONNECTED;
    if (!addr && !connected)
      e = ERR_CONN;
    else
      e = addr ? raw_sendto(s->pcb.raw, p, &a) : raw_send(s->pcb.raw, p);
  }
  pbuf_free(p);
  if (e == ERR_CONN)
    return -EDESTADDRREQ;
  return e == ERR_OK ? (long)len : lwip_errno(e);
}

long net_send(file_t *file, const struct net_iov *iov, size_t iovlen,
              const void *addr, uint32_t addrlen, int flags) {
  net_sock_t *s = sock_of(file);
  bool nonblock = (file->flags & FILE_O_NONBLOCK) || (flags & NET_MSG_DONTWAIT);
  size_t total = iov_total(iov, iovlen);

  if (s->type != SOCK_STREAM) {
    if (s->shut_wr)
      return -EPIPE;
    if (total > 0xffff)
      return -EMSGSIZE;
    uint8_t *buf = kmalloc(total ? total : 1, GFP_KERNEL);
    if (!buf)
      return -ENOMEM;
    iov_gather(iov, iovlen, 0, buf, total);
    net_lock();
    long n = dgram_send(s, buf, total, addr, addrlen);
    net_unlock();
    kfree(buf);
    return n;
  }

  if (s->state != S_CONNECTED && s->state != S_CLOSED)
    return -ENOTCONN;
  if (addr)
    return -EISCONN;
  size_t chunk = total < COPY_CHUNK ? total : COPY_CHUNK;
  uint8_t *buf = kmalloc(chunk ? chunk : 1, GFP_KERNEL);
  if (!buf)
    return -ENOMEM;

  long sent = 0;
  while ((size_t)sent < total || total == 0) {
    size_t n = total - sent < chunk ? total - sent : chunk;
    iov_gather(iov, iovlen, sent, buf, n);
    net_lock();
    long r = tcp_send_chunk(s, buf, n, nonblock,
                            (flags & NET_MSG_MORE) || sent + n < total);
    net_unlock();
    if (r < 0) {
      if (!sent)
        sent = r;
      break;
    }
    sent += r;
    if ((size_t)r < n || total == 0)
      break;
  }
  kfree(buf);
  return sent;
}

/* -- Receiving ----------------------------------------------------------- */

static bool recv_ready(void *arg) {
  net_sock_t *s = arg;
  if (s->type == SOCK_STREAM)
    return s->rx || s->eof || s->err || s->shut_rd || !s->pcb.tcp;
  return s->qcount || s->err || s->shut_rd;
}

/* TCP: up to @p len bytes into @p buf. Net lock held. */
static long tcp_recv_chunk(net_sock_t *s, uint8_t *buf, size_t len,
                           bool nonblock, bool peek) {
  long err = sock_wait(s, recv_ready, nonblock, s->rcvtimeo_ns);
  if (err)
    return err;
  if (s->rx) {
    size_t n = s->rx->tot_len < len ? s->rx->tot_len : len;
    pbuf_copy_partial(s->rx, buf, (u16_t)n, 0);
    if (!peek) {
      s->rx = pbuf_free_header(s->rx, (u16_t)n);
      if (s->pcb.tcp)
        tcp_recved(s->pcb.tcp, (u16_t)n);
    }
    return (long)n;
  }
  if (s->err)
    return take_err(s);
  return 0; /* end of stream */
}

long net_recv(file_t *file, const struct net_iov *iov, size_t iovlen,
              void *addr, uint32_t *addrlen, int flags, int *msg_flags) {
  net_sock_t *s = sock_of(file);
  bool nonblock = (file->flags & FILE_O_NONBLOCK) || (flags & NET_MSG_DONTWAIT);
  bool peek = flags & NET_MSG_PEEK;
  size_t total = iov_total(iov, iovlen);
  if (msg_flags)
    *msg_flags = 0;

  if (s->type == SOCK_STREAM) {
    if (s->state == S_NEW || s->state == S_LISTENING ||
        s->state == S_CONNECTING)
      return -ENOTCONN;
    size_t chunk = total < COPY_CHUNK ? total : COPY_CHUNK;
    uint8_t *buf = kmalloc(chunk ? chunk : 1, GFP_KERNEL);
    if (!buf)
      return -ENOMEM;
    /* Whatever is there; with MSG_WAITALL, wait until all of it. */
    bool waitall = (flags & NET_MSG_WAITALL) && !peek && !nonblock;
    long got = 0;
    do {
      size_t n = total - got < chunk ? total - got : chunk;
      net_lock();
      long r = tcp_recv_chunk(s, buf, n, nonblock || (got && !waitall), peek);
      uint32_t ip = s->pcb.tcp
                        ? ip4_addr_get_u32(ip_2_ip4(&s->pcb.tcp->remote_ip))
                        : 0;
      uint16_t port = s->pcb.tcp ? s->pcb.tcp->remote_port : 0;
      net_unlock();
      if (r <= 0) {
        if (!got)
          got = r;
        break;
      }
      iov_scatter(iov, iovlen, got, buf, r);
      if (!got)
        put_sin(addr, addrlen, ip, port);
      got += r;
    } while ((size_t)got < total && (waitall || got % chunk == 0) && !peek);
    kfree(buf);
    return got;
  }

  /* Datagrams: one per call (at most 64 KB). */
  if (total > 0x10000)
    total = 0x10000;
  uint8_t *buf = kmalloc(total ? total : 1, GFP_KERNEL);
  if (!buf)
    return -ENOMEM;
  net_lock();
  long err = sock_wait(s, recv_ready, nonblock, s->rcvtimeo_ns);
  if (!err && !s->qcount)
    err = s->err ? take_err(s) : 0; /* shut down for reading: 0 */
  dgram_t d = {0};
  size_t len = 0, n = 0;
  if (!err && s->qcount) {
    d = s->q[s->qhead];
    len = d.p->tot_len;
    n = len < total ? len : total;
    pbuf_copy_partial(d.p, buf, (u16_t)n, 0);
    if (!peek) {
      pbuf_free(d.p);
      s->qhead = (s->qhead + 1) % DGRAM_QUEUE;
      s->qcount--;
    }
  }
  net_unlock();

  if (!err) {
    iov_scatter(iov, iovlen, 0, buf, n);
    if (s->domain == NET_AF_PACKET)
      put_sll(addr, addrlen, d.proto, d.ifindex, d.pkttype, d.mac);
    else if (s->domain == NET_AF_NETLINK)
      put_snl(addr, addrlen, 0, 0); /* from the kernel */
    else
      put_sin(addr, addrlen, d.addr, d.port);
    if (len > n && msg_flags)
      *msg_flags |= NET_MSG_TRUNC;
  }
  kfree(buf);
  if (err)
    return err;
  /* MSG_TRUNC in flags: the datagram's real length, as on Linux. */
  return (long)((flags & NET_MSG_TRUNC) ? len : n);
}

/* -- Names --------------------------------------------------------------- */

long net_getname(file_t *file, void *addr, uint32_t *len, bool peer) {
  net_sock_t *s = sock_of(file);
  if (!addr || !len)
    return -EFAULT;
  long err = 0;
  net_lock();
  if (s->domain == NET_AF_NETLINK) {
    if (peer)
      put_snl(addr, len, 0, 0); /* the kernel */
    else
      put_snl(addr, len, s->nl_portid, s->nl_groups);
  } else if (s->domain == NET_AF_PACKET) {
    if (peer) {
      err = -EOPNOTSUPP;
    } else {
      struct netif *netif = net_if_by_index(s->pkt_ifindex);
      struct netdev *dev = netif ? net_if_dev(netif) : NULL;
      put_sll(addr, len, s->pkt_proto, s->pkt_ifindex, 0,
              dev ? dev->mac : NULL);
    }
  } else {
    const ip_addr_t *ip = NULL;
    uint16_t port = 0;
    bool connected = false;
    if (s->type == SOCK_STREAM && s->pcb.tcp) {
      ip = peer ? &s->pcb.tcp->remote_ip : &s->pcb.tcp->local_ip;
      port = peer ? s->pcb.tcp->remote_port : s->pcb.tcp->local_port;
      connected = s->state == S_CONNECTED;
    } else if (s->type == SOCK_DGRAM) {
      ip = peer ? &s->pcb.udp->remote_ip : &s->pcb.udp->local_ip;
      port = peer ? s->pcb.udp->remote_port : s->pcb.udp->local_port;
      connected = s->pcb.udp->flags & UDP_FLAGS_CONNECTED;
    } else if (s->type == SOCK_RAW) {
      ip = peer ? &s->pcb.raw->remote_ip : &s->pcb.raw->local_ip;
      connected = s->pcb.raw->flags & RAW_FLAGS_CONNECTED;
    }
    if (peer && !connected)
      err = -ENOTCONN;
    else
      put_sin(addr, len, ip ? ip4_addr_get_u32(ip_2_ip4(ip)) : 0, port);
  }
  net_unlock();
  return err;
}

/* -- Options ------------------------------------------------------------- */

static long put_int(void *val, uint32_t *len, int v) {
  if (!val || !len)
    return -EFAULT;
  if (*len < sizeof(int))
    return -EINVAL;
  memcpy(val, &v, sizeof(int));
  *len = sizeof(int);
  return 0;
}

static long get_int(const void *val, uint32_t len, int *v) {
  if (!val)
    return -EFAULT;
  if (len < sizeof(int))
    return -EINVAL;
  memcpy(v, val, sizeof(int));
  return 0;
}

static long put_timeo(void *val, uint32_t *len, uint64_t ns) {
  if (!val || !len)
    return -EFAULT;
  if (*len < sizeof(struct timeval_k))
    return -EINVAL;
  struct timeval_k tv = {(long)(ns / 1000000000ULL),
                         (long)(ns % 1000000000ULL / 1000)};
  memcpy(val, &tv, sizeof(tv));
  *len = sizeof(tv);
  return 0;
}

static long get_timeo(const void *val, uint32_t len, uint64_t *ns) {
  if (!val)
    return -EFAULT;
  if (len < sizeof(struct timeval_k))
    return -EINVAL;
  struct timeval_k tv;
  memcpy(&tv, val, sizeof(tv));
  if (tv.sec < 0 || tv.usec < 0 || tv.usec >= 1000000)
    return -EDOM;
  *ns = (uint64_t)tv.sec * 1000000000ULL + (uint64_t)tv.usec * 1000;
  return 0;
}

/* Apply the IP-level options kept in @p s to its pcb. Net lock held. */
static void apply_ip_opts(net_sock_t *s) {
  struct ip_pcb *pcb = NULL;
  if (s->type == SOCK_STREAM && s->pcb.tcp)
    pcb = (struct ip_pcb *)s->pcb.tcp;
  else if (s->type == SOCK_DGRAM)
    pcb = (struct ip_pcb *)s->pcb.udp;
  if (!pcb)
    return;
  if (s->reuseaddr)
    ip_set_option(pcb, SOF_REUSEADDR);
  else
    ip_reset_option(pcb, SOF_REUSEADDR);
  if (s->keepalive)
    ip_set_option(pcb, SOF_KEEPALIVE);
  else
    ip_reset_option(pcb, SOF_KEEPALIVE);
  if (s->type == SOCK_STREAM && s->pcb.tcp) {
    if (s->nodelay)
      tcp_nagle_disable(s->pcb.tcp);
    else
      tcp_nagle_enable(s->pcb.tcp);
  }
}

long net_setsockopt(file_t *file, int level, int name, const void *val,
                    uint32_t len) {
  net_sock_t *s = sock_of(file);
  int v = 0;
  long err = 0;

  if (level == SOL_SOCKET) {
    switch (name) {
    case SO_RCVTIMEO:
      return get_timeo(val, len, &s->rcvtimeo_ns);
    case SO_SNDTIMEO:
      return get_timeo(val, len, &s->sndtimeo_ns);
    case SO_BINDTODEVICE: {
      char ifname[16] = {0};
      if (!val && len)
        return -EFAULT;
      memcpy(ifname, val, len < 15 ? len : 15);
      net_lock();
      struct netif *netif = ifname[0] ? net_if_by_name(ifname) : NULL;
      if (ifname[0] && !netif)
        err = -ENODEV;
      else if (s->domain == NET_AF_NETLINK)
        ; /* nothing to bind */
      else if (s->type == SOCK_STREAM && s->pcb.tcp)
        tcp_bind_netif(s->pcb.tcp, netif);
      else if (s->type == SOCK_DGRAM)
        udp_bind_netif(s->pcb.udp, netif);
      else if (s->type == SOCK_RAW && s->domain == NET_AF_INET)
        raw_bind_netif(s->pcb.raw, netif);
      else if (s->domain == NET_AF_PACKET)
        s->pkt_ifindex = netif ? netif_get_index(netif) : 0;
      net_unlock();
      return err;
    }
    case SO_ATTACH_FILTER: /* BPF: not run; readers check what they get */
    case SO_DETACH_FILTER:
    case SO_LINGER:
    case SO_TIMESTAMP:
      return 0;
    }
    if ((err = get_int(val, len, &v)))
      return err;
    switch (name) {
    case SO_REUSEADDR:
    case SO_REUSEPORT:
      s->reuseaddr = v;
      break;
    case SO_KEEPALIVE:
      s->keepalive = v;
      break;
    case SO_BROADCAST:
      s->broadcast = v; /* lwIP sends broadcasts either way */
      return 0;
    case SO_SNDBUF:
    case SO_RCVBUF:
    case SO_RCVLOWAT:
    case SO_SNDLOWAT:
    case SO_DONTROUTE:
    case SO_OOBINLINE:
      return 0; /* fixed sizes and behaviour; hints on Linux too */
    default:
      return -ENOPROTOOPT;
    }
  } else if (level == SOL_TCP && s->type == SOCK_STREAM) {
    if ((err = get_int(val, len, &v)))
      return err;
    if (name == TCP_NODELAY)
      s->nodelay = v;
    else if (name < 1 || name > 30)
      return -ENOPROTOOPT;
    else
      return 0; /* keepalive times, MAXSEG, ...: lwIP's defaults */
  } else if (level == SOL_IP && s->domain == NET_AF_INET) {
    if ((err = get_int(val, len, &v)) && name != 4 /* IP_OPTIONS */)
      return err;
    net_lock();
    struct ip_pcb *pcb = s->type == SOCK_STREAM  ? (void *)s->pcb.tcp
                         : s->type == SOCK_DGRAM ? (void *)s->pcb.udp
                                                 : (void *)s->pcb.raw;
    if (pcb && name == IP_TTL)
      pcb->ttl = v < 0 ? 64 : (u8_t)v;
    else if (pcb && name == IP_TOS)
      pcb->tos = (u8_t)v;
    net_unlock();
    return 0; /* the rest (PKTINFO, RECVERR, MTU_DISCOVER, ...): ignored */
  } else if (level == SOL_RAW || level == SOL_PACKET ||
             level == SOL_NETLINK) {
    /* ICMP_FILTER, PACKET_AUXDATA, memberships, NETLINK_EXT_ACK and
     * NETLINK_GET_STRICT_CHK: ignored (no notifications to join) */
    return 0;
  } else {
    return -ENOPROTOOPT;
  }

  net_lock();
  apply_ip_opts(s);
  net_unlock();
  return 0;
}

long net_getsockopt(file_t *file, int level, int name, void *val,
                    uint32_t *len) {
  net_sock_t *s = sock_of(file);
  if (level == SOL_SOCKET) {
    switch (name) {
    case SO_TYPE:
      return put_int(val, len, s->type);
    case SO_DOMAIN:
      return put_int(val, len, s->domain);
    case SO_PROTOCOL:
      return put_int(val, len, s->protocol);
    case SO_ERROR: {
      net_lock();
      long e = take_err(s);
      net_unlock();
      return put_int(val, len, (int)-e);
    }
    case SO_ACCEPTCONN:
      return put_int(val, len, s->state == S_LISTENING);
    case SO_REUSEADDR:
    case SO_REUSEPORT:
      return put_int(val, len, s->reuseaddr);
    case SO_KEEPALIVE:
      return put_int(val, len, s->keepalive);
    case SO_BROADCAST:
      return put_int(val, len, s->broadcast);
    case SO_SNDBUF:
      return put_int(val, len, TCP_SND_BUF);
    case SO_RCVBUF:
      return put_int(val, len, TCP_WND);
    case SO_RCVTIMEO:
      return put_timeo(val, len, s->rcvtimeo_ns);
    case SO_SNDTIMEO:
      return put_timeo(val, len, s->sndtimeo_ns);
    }
  } else if (level == SOL_TCP && s->type == SOCK_STREAM) {
    if (name == TCP_NODELAY)
      return put_int(val, len, s->nodelay);
    if (name == TCP_MAXSEG)
      return put_int(val, len, TCP_MSS);
  } else if (level == SOL_IP && name == IP_TTL) {
    return put_int(val, len, IP_DEFAULT_TTL);
  }
  return -ENOPROTOOPT;
}

/* -- File operations ----------------------------------------------------- */

static long sock_read(file_t *file, void *buf, size_t len) {
  struct net_iov iov = {buf, len};
  return net_recv(file, &iov, 1, NULL, NULL, 0, NULL);
}

static long sock_write(file_t *file, const void *buf, size_t len) {
  struct net_iov iov = {(void *)buf, len};
  return net_send(file, &iov, 1, NULL, 0, 0);
}

static short sock_poll(file_t *file) {
  net_sock_t *s = sock_of(file);
  short ev = 0;
  net_lock();
  if (s->type == SOCK_STREAM) {
    if (s->state == S_LISTENING) {
      if (s->nbacklog)
        ev |= FILE_POLLIN;
    } else if (s->state == S_CONNECTED || s->state == S_CLOSED) {
      if (s->rx || s->eof || s->shut_rd)
        ev |= FILE_POLLIN;
      if (s->can_send || s->shut_wr || !s->pcb.tcp)
        ev |= FILE_POLLOUT;
      if (s->state == S_CLOSED)
        ev |= FILE_POLLHUP;
    } else if (s->state == S_NEW) {
      ev |= FILE_POLLOUT | FILE_POLLHUP; /* as Linux: never connected */
    }
    if (s->err)
      ev |= FILE_POLLERR;
  } else {
    if (s->qcount || s->shut_rd)
      ev |= FILE_POLLIN;
    ev |= FILE_POLLOUT;
    if (s->err)
      ev |= FILE_POLLERR;
  }
  net_unlock();
  return ev;
}

static struct wait_queue *sock_wait_queue(file_t *file) {
  return &sock_of(file)->wq;
}

static long sock_ioctl(file_t *file, unsigned long request, void *arg) {
  net_sock_t *s = sock_of(file);
  if (request == FIONREAD) {
    if (!arg)
      return -EFAULT;
    net_lock();
    int n = 0;
    if (s->type == SOCK_STREAM)
      n = s->rx ? s->rx->tot_len : 0;
    else if (s->qcount)
      n = s->q[s->qhead].p->tot_len;
    net_unlock();
    memcpy(arg, &n, sizeof(n));
    return 0;
  }
  if (request == FIONBIO) {
    if (!arg)
      return -EFAULT;
    int on;
    memcpy(&on, arg, sizeof(on));
    if (on)
      file->flags |= FILE_O_NONBLOCK;
    else
      file->flags &= ~FILE_O_NONBLOCK;
    return 0;
  }
  return net_if_ioctl(request, arg);
}

static void sock_stat(file_t *file, uint32_t *type, uint64_t *size) {
  (void)file;
  *type = FILE_S_IFSOCK;
  *size = 0;
}

static const file_ops_t net_sock_ops = {
    .read = sock_read,
    .write = sock_write,
    .poll = sock_poll,
    .wait_queue = sock_wait_queue,
    .ioctl = sock_ioctl,
    .stat = sock_stat,
    .release = sock_release,
};

/* -- /proc/net ----------------------------------------------------------- */

/* Linux's TCP state numbers, by lwIP's. */
static const uint8_t linux_tcp_state[] = {
    [CLOSED] = 7,     [LISTEN] = 10,     [SYN_SENT] = 2,   [SYN_RCVD] = 3,
    [ESTABLISHED] = 1, [FIN_WAIT_1] = 4, [FIN_WAIT_2] = 5, [CLOSE_WAIT] = 8,
    [CLOSING] = 11,   [LAST_ACK] = 9,    [TIME_WAIT] = 6,
};
#define LINUX_TCP_ESTABLISHED 1
#define LINUX_TCP_CLOSE 7

static const char proc_v4_head[] =
    "  sl  local_address rem_address   st tx_queue rx_queue tr tm->when "
    "retrnsmt   uid  timeout inode\n";
static const char proc_v6_head[] =
    "  sl  local_address                         remote_address              "
    "          st tx_queue rx_queue tr tm->when retrnsmt   uid  timeout "
    "inode\n";

/* Unread bytes a socket holds. */
static uint32_t sock_unread(const net_sock_t *s) {
  if (!s)
    return 0;
  uint32_t n = s->rx ? s->rx->tot_len : 0;
  for (int i = 0; i < s->qcount; i++)
    n += s->q[(s->qhead + i) % DGRAM_QUEUE].p->tot_len;
  return n;
}

/* One line, as Linux writes it: addresses as the raw network-order word
 * in hex, ports in host order. */
static void proc_line(proc_out_t *o, int *sl, const ip_addr_t *local,
                      uint16_t lport, const ip_addr_t *remote,
                      uint16_t rport, int state, uint32_t tx, uint32_t rx) {
  PROC_PRINT(o,
             "%4d: %08X:%04X %08X:%04X %02X %08X:%08X 00:00000000 00000000 "
             "    0        0 0 1 0000000000000000\n",
             (*sl)++, ip4_addr_get_u32(ip_2_ip4(local)), lport,
             ip4_addr_get_u32(ip_2_ip4(remote)), rport, state, tx, rx);
}

size_t net_proc_tcp(char *buf, size_t size, bool v6) {
  proc_out_t o = {buf, size, 0};
  PROC_PRINT(&o, "%s", v6 ? proc_v6_head : proc_v4_head);
  if (v6)
    return proc_len(&o);
  int sl = 0;
  net_lock();
  for (struct tcp_pcb_listen *l = tcp_listen_pcbs.listen_pcbs; l;
       l = l->next)
    proc_line(&o, &sl, &l->local_ip, l->local_port, IP4_ADDR_ANY, 0,
              linux_tcp_state[LISTEN], 0, 0);
  /* Bound and not listening yet, connections, and those in TIME_WAIT. */
  struct tcp_pcb *lists[] = {tcp_bound_pcbs, tcp_active_pcbs, tcp_tw_pcbs};
  for (size_t i = 0; i < sizeof(lists) / sizeof(lists[0]); i++) {
    for (struct tcp_pcb *p = lists[i]; p; p = p->next) {
      /* The socket that owns it, if any is left (closed ones are not
       * reached from the pcb any more). */
      net_sock_t *s = p->callback_arg;
      /* Sent, not acknowledged yet. Once the FIN is acknowledged lastack
       * is past the data, so the difference goes negative: none left. */
      int32_t unacked = (int32_t)(p->snd_lbb - p->lastack);
      if (p->state == CLOSED || unacked < 0)
        unacked = 0;
      proc_line(&o, &sl, &p->local_ip, p->local_port, &p->remote_ip,
                p->remote_port, linux_tcp_state[p->state], (uint32_t)unacked,
                sock_unread(s));
    }
  }
  net_unlock();
  return proc_len(&o);
}

size_t net_proc_udp(char *buf, size_t size, bool v6) {
  proc_out_t o = {buf, size, 0};
  PROC_PRINT(&o, "%s", v6 ? proc_v6_head : proc_v4_head);
  if (v6)
    return proc_len(&o);
  int sl = 0;
  net_lock();
  for (struct udp_pcb *p = udp_pcbs; p; p = p->next) {
    int state = (p->flags & UDP_FLAGS_CONNECTED) ? LINUX_TCP_ESTABLISHED
                                                 : LINUX_TCP_CLOSE;
    proc_line(&o, &sl, &p->local_ip, p->local_port, &p->remote_ip,
              p->remote_port, state, 0, sock_unread(p->recv_arg));
  }
  net_unlock();
  return proc_len(&o);
}

/* Raw sockets: the "port" is the protocol, as on Linux. */
size_t net_proc_raw(char *buf, size_t size, bool v6) {
  proc_out_t o = {buf, size, 0};
  PROC_PRINT(&o, "%s", v6 ? proc_v6_head : proc_v4_head);
  if (v6)
    return proc_len(&o);
  int sl = 0;
  net_lock();
  for (net_sock_t *s = listed; s; s = s->next_listed) {
    if (s->domain != NET_AF_INET || !s->pcb.raw)
      continue;
    struct raw_pcb *p = s->pcb.raw;
    proc_line(&o, &sl, &p->local_ip, (uint16_t)s->protocol, &p->remote_ip, 0,
              LINUX_TCP_CLOSE, 0, sock_unread(s));
  }
  net_unlock();
  return proc_len(&o);
}
