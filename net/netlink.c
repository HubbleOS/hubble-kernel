/*
 * rtnetlink (NETLINK_ROUTE): what `ip addr`, `ip link` and `ip route`
 * talk to the kernel with.
 *
 * A request is handled whole when it is sent; the replies (a dump split
 * into datagrams, an ack, an error) are handed to the socket's receive
 * queue through @p reply. Supported:
 *
 *   RTM_GETLINK   dump, or one interface by index or IFLA_IFNAME
 *   RTM_GETADDR   dump (IPv4: lwIP has one address per interface)
 *   RTM_NEWADDR   set the interface's address and prefix
 *   RTM_DELADDR   remove it
 *   RTM_GETROUTE  dump: the default route and each interface's network
 *   RTM_NEWROUTE, RTM_DELROUTE   what net_route_change() can do
 *
 * Anything else gets NLMSG_ERROR with -EOPNOTSUPP. Link changes (up,
 * down, MTU) go through the SIOCSIF* ioctls, which is what busybox's
 * `ip link set` uses. No multicast notifications are sent.
 */
#include "net.h"

#include "lwip/netif.h"

#include <hubble/errno.h>
#include <hubble/printk.h>
#include <hubble/string.h>

/* -- The ABI (linux/netlink.h, linux/rtnetlink.h) ------------------------ */

struct nlmsghdr {
  uint32_t len;
  uint16_t type;
  uint16_t flags;
  uint32_t seq;
  uint32_t pid;
};

struct nlmsgerr {
  int32_t error;
  struct nlmsghdr msg;
};

struct rtattr {
  uint16_t len;
  uint16_t type;
};

struct ifinfomsg {
  uint8_t family, pad;
  uint16_t type;
  int32_t index;
  uint32_t flags, change;
};

struct ifaddrmsg {
  uint8_t family, prefixlen, flags, scope;
  uint32_t index;
};

struct rtmsg {
  uint8_t family, dst_len, src_len, tos;
  uint8_t table, protocol, scope, type;
  uint32_t flags;
};

#define NL_ALIGN(n) (((n) + 3u) & ~3u)
#define NL_HDRLEN NL_ALIGN(sizeof(struct nlmsghdr))
#define RTA_HDRLEN NL_ALIGN(sizeof(struct rtattr))

#define NLMSG_ERROR 2
#define NLMSG_DONE 3

#define NLM_F_REQUEST 0x1
#define NLM_F_MULTI 0x2
#define NLM_F_ACK 0x4
#define NLM_F_DUMP 0x300 /* NLM_F_ROOT | NLM_F_MATCH */
#define NLM_F_EXCL 0x200 /* new requests */

#define RTM_NEWLINK 16
#define RTM_GETLINK 18
#define RTM_NEWADDR 20
#define RTM_DELADDR 21
#define RTM_GETADDR 22
#define RTM_NEWROUTE 24
#define RTM_DELROUTE 25
#define RTM_GETROUTE 26

#define AF_UNSPEC 0
#define AF_INET 2
#define ARPHRD_ETHER 1
#define ARPHRD_LOOPBACK 772

#define IFLA_ADDRESS 1
#define IFLA_BROADCAST 2
#define IFLA_IFNAME 3
#define IFLA_MTU 4
#define IFLA_QDISC 6
#define IFLA_TXQLEN 13
#define IFLA_OPERSTATE 16
#define IFLA_LINKMODE 17
#define IF_OPER_DOWN 2
#define IF_OPER_UP 6
#define IF_OPER_UNKNOWN 0 /* lo's, as on Linux */

#define IFA_ADDRESS 1
#define IFA_LOCAL 2
#define IFA_LABEL 3
#define IFA_BROADCAST 4
#define IFA_F_PERMANENT 0x80

#define RTA_DST 1
#define RTA_OIF 4
#define RTA_GATEWAY 5
#define RTA_PREFSRC 7
#define RTA_TABLE 15

#define RT_TABLE_MAIN 254
#define RTPROT_KERNEL 2 /* routes that come with an address */
#define RTPROT_BOOT 3   /* routes someone added */
#define RT_SCOPE_UNIVERSE 0
#define RT_SCOPE_LINK 253
#define RT_SCOPE_HOST 254
#define RTN_UNICAST 1

/* -- Building replies ---------------------------------------------------- */

/* Replies go out as datagrams of at most this size, split between
 * messages: busybox reads into an 8 KiB buffer. */
#define NL_DGRAM_MAX 4096
#define NL_MSG_MAX 512 /* one link, address or route */

typedef struct {
  uint8_t dgram[NL_DGRAM_MAX];
  size_t len;
  uint32_t seq, portid;
  net_nl_reply_fn reply;
  void *ctx;
  long err; /* the first reply that could not be queued */
} nl_out_t;

typedef struct {
  uint8_t b[NL_MSG_MAX];
  size_t len;
} nl_msg_t;

static void out_flush(nl_out_t *o) {
  if (o->len && !o->err)
    o->err = o->reply(o->ctx, o->dgram, o->len);
  o->len = 0;
}

static void *msg_put(nl_msg_t *m, const void *data, size_t len) {
  void *at = m->b + m->len;
  if (m->len + NL_ALIGN(len) > sizeof(m->b))
    return NULL; /* not reached: messages are small */
  memset(at, 0, NL_ALIGN(len));
  if (data)
    memcpy(at, data, len);
  m->len += NL_ALIGN(len);
  return at;
}

static void attr_put(nl_msg_t *m, uint16_t type, const void *data,
                     size_t len) {
  struct rtattr a = {.len = (uint16_t)(RTA_HDRLEN + len), .type = type};
  msg_put(m, &a, sizeof(a));
  msg_put(m, data, len);
}

static void attr_u32(nl_msg_t *m, uint16_t type, uint32_t v) {
  attr_put(m, type, &v, sizeof(v));
}

static void attr_str(nl_msg_t *m, uint16_t type, const char *s) {
  attr_put(m, type, s, strlen(s) + 1);
}

static void msg_start(nl_msg_t *m, nl_out_t *o, uint16_t type,
                      uint16_t flags) {
  m->len = 0;
  struct nlmsghdr h = {
      .type = type, .flags = flags, .seq = o->seq, .pid = o->portid};
  msg_put(m, &h, sizeof(h));
}

static void msg_end(nl_out_t *o, nl_msg_t *m) {
  ((struct nlmsghdr *)m->b)->len = (uint32_t)m->len;
  if (o->len + m->len > sizeof(o->dgram))
    out_flush(o);
  memcpy(o->dgram + o->len, m->b, m->len);
  o->len += m->len;
}

static void send_done(nl_out_t *o) {
  nl_msg_t m;
  msg_start(&m, o, NLMSG_DONE, NLM_F_MULTI);
  int32_t zero = 0;
  msg_put(&m, &zero, sizeof(zero));
  msg_end(o, &m);
}

/* An ack (@p err 0) or an error, with the request's header. */
static void send_error(nl_out_t *o, const struct nlmsghdr *req, long err) {
  nl_msg_t m;
  msg_start(&m, o, NLMSG_ERROR, 0);
  struct nlmsgerr e = {.error = (int32_t)err, .msg = *req};
  msg_put(&m, &e, sizeof(e));
  msg_end(o, &m);
}

/* -- Reading requests ---------------------------------------------------- */

/* The attribute @p type after a fixed header of @p fixed bytes, or NULL. */
static const void *attr_find(const struct nlmsghdr *h, size_t fixed,
                             uint16_t type, size_t *len) {
  size_t off = NL_HDRLEN + NL_ALIGN(fixed);
  while (off + sizeof(struct rtattr) <= h->len) {
    const struct rtattr *a = (const void *)((const uint8_t *)h + off);
    if (a->len < sizeof(*a) || off + a->len > h->len)
      break;
    if (a->type == type) {
      *len = a->len - RTA_HDRLEN;
      return (const uint8_t *)a + RTA_HDRLEN;
    }
    off += NL_ALIGN(a->len);
  }
  return NULL;
}

static bool attr_get_u32(const struct nlmsghdr *h, size_t fixed,
                         uint16_t type, uint32_t *v) {
  size_t len;
  const void *p = attr_find(h, fixed, type, &len);
  if (!p || len < sizeof(*v))
    return false;
  memcpy(v, p, sizeof(*v));
  return true;
}

static const void *body(const struct nlmsghdr *h, size_t size) {
  return h->len >= NL_HDRLEN + size ? (const uint8_t *)h + NL_HDRLEN : NULL;
}

/* -- Links --------------------------------------------------------------- */

static uint32_t mask_of(uint8_t prefix) {
  return prefix ? PP_HTONL(0xffffffffUL << (32 - prefix)) : 0;
}

static uint8_t prefix_of(uint32_t mask) {
  return (uint8_t)__builtin_popcount(mask); /* contiguous masks only */
}

static void put_link(nl_out_t *o, struct netif *netif, uint16_t flags) {
  struct netdev *dev = net_if_dev(netif);
  nl_msg_t m;
  msg_start(&m, o, RTM_NEWLINK, flags);
  struct ifinfomsg ifi = {
      .family = AF_UNSPEC,
      .type = dev ? ARPHRD_ETHER : ARPHRD_LOOPBACK,
      .index = netif_get_index(netif),
      .flags = net_if_flags(netif),
      .change = 0xffffffff,
  };
  msg_put(&m, &ifi, sizeof(ifi));
  attr_str(&m, IFLA_IFNAME, net_if_name(netif));
  attr_u32(&m, IFLA_TXQLEN, dev ? 1000 : 0);
  uint8_t oper = !dev                    ? IF_OPER_UNKNOWN
                 : (ifi.flags & 0x10000) ? IF_OPER_UP
                                         : IF_OPER_DOWN;
  attr_put(&m, IFLA_OPERSTATE, &oper, 1);
  uint8_t mode = 0;
  attr_put(&m, IFLA_LINKMODE, &mode, 1);
  attr_u32(&m, IFLA_MTU, netif->mtu);
  attr_str(&m, IFLA_QDISC, dev ? "pfifo_fast" : "noqueue");
  static const uint8_t zero[6], bcast[6] = {0xff, 0xff, 0xff,
                                            0xff, 0xff, 0xff};
  attr_put(&m, IFLA_ADDRESS, dev ? dev->mac : zero, 6);
  attr_put(&m, IFLA_BROADCAST, dev ? bcast : zero, 6);
  msg_end(o, &m);
}

static long get_link(nl_out_t *o, const struct nlmsghdr *h) {
  if ((h->flags & NLM_F_DUMP) == NLM_F_DUMP) {
    struct netif *netif;
    NETIF_FOREACH(netif) { put_link(o, netif, NLM_F_MULTI); }
    send_done(o);
    return 0;
  }
  const struct ifinfomsg *ifi = body(h, sizeof(*ifi));
  if (!ifi)
    return -EINVAL;
  struct netif *netif = NULL;
  size_t len;
  const char *name = attr_find(h, sizeof(*ifi), IFLA_IFNAME, &len);
  if (ifi->index)
    netif = net_if_by_index(ifi->index);
  else if (name && len && len <= 16 && !name[len - 1])
    netif = net_if_by_name(name);
  if (!netif)
    return -ENODEV;
  put_link(o, netif, 0);
  return 0;
}

/* -- Addresses ----------------------------------------------------------- */

static void put_addr(nl_out_t *o, struct netif *netif) {
  uint32_t a = ip4_addr_get_u32(netif_ip4_addr(netif));
  uint32_t mask = ip4_addr_get_u32(netif_ip4_netmask(netif));
  bool lo = !net_if_dev(netif);
  nl_msg_t m;
  msg_start(&m, o, RTM_NEWADDR, NLM_F_MULTI);
  struct ifaddrmsg ifa = {
      .family = AF_INET,
      .prefixlen = prefix_of(mask),
      .flags = IFA_F_PERMANENT,
      .scope = lo ? RT_SCOPE_HOST : RT_SCOPE_UNIVERSE,
      .index = netif_get_index(netif),
  };
  msg_put(&m, &ifa, sizeof(ifa));
  attr_u32(&m, IFA_ADDRESS, a);
  attr_u32(&m, IFA_LOCAL, a);
  if (!lo)
    attr_u32(&m, IFA_BROADCAST, (a & mask) | ~mask);
  attr_str(&m, IFA_LABEL, net_if_name(netif));
  msg_end(o, &m);
}

static long get_addr(nl_out_t *o, const struct nlmsghdr *h) {
  if ((h->flags & NLM_F_DUMP) != NLM_F_DUMP)
    return -EOPNOTSUPP; /* as Linux: addresses are only dumped */
  const struct ifaddrmsg *req = body(h, 1);
  if (!req || req->family == AF_UNSPEC || req->family == AF_INET) {
    struct netif *netif;
    NETIF_FOREACH(netif) {
      if (!ip4_addr_isany_val(*netif_ip4_addr(netif)))
        put_addr(o, netif);
    }
  }
  send_done(o);
  return 0;
}

/* The interface and address of an RTM_NEWADDR / RTM_DELADDR. */
static long addr_request(const struct nlmsghdr *h, struct netif **netif,
                         uint32_t *addr, uint8_t *prefix, bool *has_addr) {
  const struct ifaddrmsg *ifa = body(h, sizeof(*ifa));
  if (!ifa)
    return -EINVAL;
  if (ifa->family != AF_INET)
    return -EAFNOSUPPORT;
  if (ifa->prefixlen > 32)
    return -EINVAL;
  *netif = net_if_by_index((int)ifa->index);
  if (!*netif)
    return -ENODEV;
  *prefix = ifa->prefixlen;
  *has_addr = attr_get_u32(h, sizeof(*ifa), IFA_LOCAL, addr) ||
              attr_get_u32(h, sizeof(*ifa), IFA_ADDRESS, addr);
  return 0;
}

static long new_addr(const struct nlmsghdr *h) {
  struct netif *netif;
  uint32_t a = 0;
  uint8_t prefix;
  bool has;
  long err = addr_request(h, &netif, &a, &prefix, &has);
  if (err)
    return err;
  if (!has || !a)
    return -EINVAL;

  uint32_t cur = ip4_addr_get_u32(netif_ip4_addr(netif));
  uint32_t curmask = ip4_addr_get_u32(netif_ip4_netmask(netif));
  if (cur == a && curmask == mask_of(prefix))
    return (h->flags & NLM_F_EXCL) ? -EEXIST : 0;
  if (cur && cur != a)
    /* Linux would add a second address; lwIP keeps one. */
    printk(KERN_INFO "net: %s: one IPv4 address per interface, "
                     "%u.%u.%u.%u replaces %u.%u.%u.%u\n",
           net_if_name(netif), a & 0xff, a >> 8 & 0xff, a >> 16 & 0xff,
           a >> 24, cur & 0xff, cur >> 8 & 0xff, cur >> 16 & 0xff,
           cur >> 24);

  ip4_addr_t ip, mask;
  ip4_addr_set_u32(&ip, a);
  ip4_addr_set_u32(&mask, mask_of(prefix));
  netif_set_addr(netif, &ip, &mask, netif_ip4_gw(netif));
  return 0;
}

static long del_addr(const struct nlmsghdr *h) {
  struct netif *netif;
  uint32_t a = 0;
  uint8_t prefix;
  bool has;
  long err = addr_request(h, &netif, &a, &prefix, &has);
  if (err)
    return err;
  uint32_t cur = ip4_addr_get_u32(netif_ip4_addr(netif));
  if (!cur || (has && a != cur))
    return -EADDRNOTAVAIL;
  /* The gateway went through this address: it goes too, as on Linux. */
  netif_set_addr(netif, IP4_ADDR_ANY4, IP4_ADDR_ANY4, IP4_ADDR_ANY4);
  return 0;
}

/* -- Routes -------------------------------------------------------------- */

static void put_route(nl_out_t *o, struct netif *netif, uint32_t dst,
                      uint8_t dst_len, uint32_t gw, uint32_t src) {
  nl_msg_t m;
  msg_start(&m, o, RTM_NEWROUTE, NLM_F_MULTI);
  struct rtmsg rt = {
      .family = AF_INET,
      .dst_len = dst_len,
      .table = RT_TABLE_MAIN,
      .protocol = gw ? RTPROT_BOOT : RTPROT_KERNEL,
      .scope = gw ? RT_SCOPE_UNIVERSE : RT_SCOPE_LINK,
      .type = RTN_UNICAST,
  };
  msg_put(&m, &rt, sizeof(rt));
  attr_u32(&m, RTA_TABLE, RT_TABLE_MAIN);
  if (dst_len)
    attr_u32(&m, RTA_DST, dst);
  if (src)
    attr_u32(&m, RTA_PREFSRC, src);
  if (gw)
    attr_u32(&m, RTA_GATEWAY, gw);
  attr_u32(&m, RTA_OIF, netif_get_index(netif));
  msg_end(o, &m);
}

static long get_route(nl_out_t *o, const struct nlmsghdr *h) {
  if ((h->flags & NLM_F_DUMP) != NLM_F_DUMP)
    return -EOPNOTSUPP; /* ip route get: not supported */
  const struct rtmsg *req = body(h, 1);
  if (!req || req->family == AF_UNSPEC || req->family == AF_INET) {
    /* What /proc/net/route lists: lo's routes are in the local table. */
    struct netif *d = netif_default;
    if (d && netif_is_up(d) && !ip4_addr_isany_val(*netif_ip4_gw(d)))
      put_route(o, d, 0, 0, ip4_addr_get_u32(netif_ip4_gw(d)), 0);
    struct netif *netif;
    NETIF_FOREACH(netif) {
      uint32_t a = ip4_addr_get_u32(netif_ip4_addr(netif));
      uint32_t m = ip4_addr_get_u32(netif_ip4_netmask(netif));
      if (net_if_dev(netif) && a && netif_is_up(netif))
        put_route(o, netif, a & m, prefix_of(m), 0, a);
    }
  }
  send_done(o);
  return 0;
}

static long change_route(const struct nlmsghdr *h, bool add) {
  const struct rtmsg *rt = body(h, sizeof(*rt));
  if (!rt)
    return -EINVAL;
  if (rt->family != AF_INET)
    return -EAFNOSUPPORT;
  if (rt->dst_len > 32)
    return -EINVAL;
  if (rt->table != RT_TABLE_MAIN && rt->table != 0)
    return -EOPNOTSUPP; /* one table */
  uint32_t dst = 0, gw = 0, oif = 0;
  attr_get_u32(h, sizeof(*rt), RTA_DST, &dst);
  attr_get_u32(h, sizeof(*rt), RTA_GATEWAY, &gw);
  struct netif *netif = NULL;
  if (attr_get_u32(h, sizeof(*rt), RTA_OIF, &oif)) {
    netif = net_if_by_index((int)oif);
    if (!netif)
      return -ENODEV;
  }
  uint32_t mask = mask_of(rt->dst_len);
  return net_route_change(add, dst & mask, mask, gw, netif);
}

/* -- Requests ------------------------------------------------------------ */

long net_rtnl_request(const void *buf, size_t len, uint32_t portid,
                      net_nl_reply_fn reply, void *ctx) {
  /* Static: a whole datagram is too big for the stack. Under the lock. */
  static nl_out_t o;
  o.len = 0;
  o.portid = portid;
  o.reply = reply;
  o.ctx = ctx;
  o.err = 0;

  size_t off = 0;
  while (off + sizeof(struct nlmsghdr) <= len) {
    const struct nlmsghdr *h = (const void *)((const uint8_t *)buf + off);
    if (h->len < sizeof(*h) || off + h->len > len)
      break; /* malformed: the rest is dropped, as on Linux */
    off += NL_ALIGN(h->len);
    if (!(h->flags & NLM_F_REQUEST))
      continue; /* not for the kernel */
    o.seq = h->seq;

    long err;
    switch (h->type) {
    case RTM_GETLINK:
      err = get_link(&o, h);
      break;
    case RTM_GETADDR:
      err = get_addr(&o, h);
      break;
    case RTM_GETROUTE:
      err = get_route(&o, h);
      break;
    case RTM_NEWADDR:
      err = new_addr(h);
      break;
    case RTM_DELADDR:
      err = del_addr(h);
      break;
    case RTM_NEWROUTE:
      err = change_route(h, true);
      break;
    case RTM_DELROUTE:
      err = change_route(h, false);
      break;
    default:
      err = -EOPNOTSUPP;
    }
    if (err || (h->flags & NLM_F_ACK))
      send_error(&o, h, err);
  }
  out_flush(&o);
  return o.err;
}
