/*
 * Interface and routing ioctls (Linux SIOC*), on any AF_INET or AF_PACKET
 * socket: what ifconfig, route and udhcpc use to list interfaces, read
 * their MAC, and set addresses, flags and the default gateway.
 *
 * lwIP has no routing table: it reaches the networks of its interfaces
 * directly and sends everything else to the default interface's gateway.
 * So the only route that can be added is a default route (and routes to
 * an interface's own network, which exist anyway).
 */
#include "net.h"
#include "socket.h"

#include "lwip/netif.h"

#include <hubble/errno.h>
#include <hubble/string.h>

#define SIOCADDRT 0x890B
#define SIOCDELRT 0x890C
#define SIOCGIFNAME 0x8910
#define SIOCGIFCONF 0x8912
#define SIOCGIFFLAGS 0x8913
#define SIOCSIFFLAGS 0x8914
#define SIOCGIFADDR 0x8915
#define SIOCSIFADDR 0x8916
#define SIOCGIFDSTADDR 0x8917
#define SIOCGIFBRDADDR 0x8919
#define SIOCSIFBRDADDR 0x891A
#define SIOCGIFNETMASK 0x891B
#define SIOCSIFNETMASK 0x891C
#define SIOCGIFMETRIC 0x891D
#define SIOCSIFMETRIC 0x891E
#define SIOCGIFMTU 0x8921
#define SIOCSIFMTU 0x8922
#define SIOCGIFHWADDR 0x8927
#define SIOCGIFINDEX 0x8933
#define SIOCGIFTXQLEN 0x8942
#define SIOCSIFTXQLEN 0x8943
#define SIOCGIFMAP 0x8970
#define SIOCSIFMAP 0x8971

#define IFF_UP 0x1
#define IFF_BROADCAST 0x2
#define IFF_LOOPBACK 0x8
#define IFF_RUNNING 0x40
#define IFF_MULTICAST 0x1000
#define IFF_LOWER_UP 0x10000 /* netlink only: ifreq flags are 16 bits */

#define RTF_UP 0x1
#define RTF_GATEWAY 0x2

#define AF_INET 2
#define ARPHRD_ETHER 1
#define ARPHRD_LOOPBACK 772

#define IFNAMSIZ 16

struct sockaddr_gen {
  uint16_t family;
  char data[14];
};

struct sockaddr_in_k {
  uint16_t sin_family;
  uint16_t sin_port;
  uint32_t sin_addr;
  uint8_t zero[8];
};

struct ifreq {
  char name[IFNAMSIZ];
  union {
    struct sockaddr_gen addr;
    short flags;
    int ivalue;
    char newname[IFNAMSIZ];
    uint8_t raw[24];
  };
};

struct ifconf {
  int len;
  struct ifreq *req;
};

struct rtentry {
  unsigned long pad1;
  struct sockaddr_gen dst;
  struct sockaddr_gen gateway;
  struct sockaddr_gen genmask;
  unsigned short flags;
  short pad2;
  unsigned long pad3;
  void *pad4;
  short metric;
  char *dev;
  unsigned long mtu;
  unsigned long window;
  unsigned short irtt;
};

static void put_in(struct sockaddr_gen *sa, uint32_t addr) {
  struct sockaddr_in_k *in = (struct sockaddr_in_k *)sa;
  memset(in, 0, sizeof(*in));
  in->sin_family = AF_INET;
  in->sin_addr = addr;
}

static long get_in(const struct sockaddr_gen *sa, uint32_t *addr) {
  if (sa->family != AF_INET)
    return -EINVAL;
  *addr = ((const struct sockaddr_in_k *)sa)->sin_addr;
  return 0;
}

unsigned net_if_flags(struct netif *netif) {
  unsigned f = 0;
  if (netif_is_up(netif))
    f |= IFF_UP;
  if (netif_is_up(netif) && netif_is_link_up(netif))
    f |= IFF_RUNNING | IFF_LOWER_UP;
  if (net_if_dev(netif))
    f |= IFF_BROADCAST | IFF_MULTICAST;
  else
    f |= IFF_LOOPBACK;
  return f;
}

/* ifconfig 10.0.2.15 also gives the interface the netmask of the
 * address's class, as Linux does (ifconfig sets its own after). */
static uint32_t class_mask(uint32_t addr) {
  uint8_t first = addr & 0xff; /* network order: the first byte */
  if (first < 128)
    return PP_HTONL(0xff000000UL);
  if (first < 192)
    return PP_HTONL(0xffff0000UL);
  return PP_HTONL(0xffffff00UL);
}

static long list_ifs(struct ifconf *ifc) {
  if (!ifc)
    return -EFAULT;
  int max = ifc->len / (int)sizeof(struct ifreq), n = 0;
  struct netif *netif;
  NETIF_FOREACH(netif) {
    /* Like Linux, only interfaces with an address. */
    if (ip4_addr_isany_val(*netif_ip4_addr(netif)))
      continue;
    if (ifc->req) {
      if (n == max)
        break;
      struct ifreq *r = &ifc->req[n];
      memset(r, 0, sizeof(*r));
      strncpy(r->name, net_if_name(netif), IFNAMSIZ - 1);
      put_in(&r->addr, ip4_addr_get_u32(netif_ip4_addr(netif)));
    }
    n++;
  }
  ifc->len = n * (int)sizeof(struct ifreq);
  return 0;
}

long net_route_change(bool add, uint32_t dst, uint32_t mask, uint32_t gw,
                      struct netif *netif) {
  if (dst != 0 || mask != 0) {
    /* A route to a network: only one an interface already reaches. */
    struct netif *n;
    NETIF_FOREACH(n) {
      uint32_t a = ip4_addr_get_u32(netif_ip4_addr(n));
      uint32_t m = ip4_addr_get_u32(netif_ip4_netmask(n));
      if (!gw && a && m == mask && (a & m) == (dst & mask) &&
          (!netif || netif == n))
        return add ? 0 : -EOPNOTSUPP; /* it goes with the address */
    }
    return -EOPNOTSUPP; /* lwIP has no routing table */
  }

  /* The default route: via the interface on the gateway's network. */
  if (!netif && gw) {
    struct netif *n;
    NETIF_FOREACH(n) {
      uint32_t a = ip4_addr_get_u32(netif_ip4_addr(n));
      uint32_t m = ip4_addr_get_u32(netif_ip4_netmask(n));
      if (net_if_dev(n) && a && (a & m) == (gw & m)) {
        netif = n;
        break;
      }
    }
  }
  if (!netif)
    netif = netif_default;
  if (!netif)
    return -ENETUNREACH;

  if (!add) {
    uint32_t cur = ip4_addr_get_u32(netif_ip4_gw(netif));
    if (!cur || (gw && gw != cur))
      return -ESRCH;
  }
  ip4_addr_t g;
  ip4_addr_set_u32(&g, add ? gw : 0);
  netif_set_gw(netif, &g);
  if (add)
    netif_set_default(netif);
  return 0;
}

/* route(8)'s rtentry. As Linux: the destination must be AF_INET; the mask
 * and gateway may also be left unset (family 0), which busybox does for
 * "default". */
static long route(unsigned long request, struct rtentry *rt) {
  if (!rt)
    return -EFAULT;
  uint32_t dst, mask = 0, gw = 0;
  if (get_in(&rt->dst, &dst))
    return -EAFNOSUPPORT;
  if (get_in(&rt->genmask, &mask) && rt->genmask.family)
    return -EAFNOSUPPORT;
  if ((rt->flags & RTF_GATEWAY) && get_in(&rt->gateway, &gw))
    return -EAFNOSUPPORT;

  struct netif *netif = NULL;
  if (rt->dev) {
    char name[IFNAMSIZ];
    strncpy(name, rt->dev, IFNAMSIZ - 1);
    name[IFNAMSIZ - 1] = '\0';
    netif = net_if_by_name(name);
    if (!netif)
      return -ENODEV;
  }
  return net_route_change(request == SIOCADDRT, dst, mask, gw, netif);
}

static long if_request(unsigned long request, struct ifreq *r) {
  if (!r)
    return -EFAULT;

  if (request == SIOCGIFNAME) {
    struct netif *netif = net_if_by_index(r->ivalue);
    if (!netif)
      return -ENODEV;
    memset(r->name, 0, IFNAMSIZ);
    strncpy(r->name, net_if_name(netif), IFNAMSIZ - 1);
    return 0;
  }

  char name[IFNAMSIZ];
  memcpy(name, r->name, IFNAMSIZ);
  name[IFNAMSIZ - 1] = '\0';
  struct netif *netif = net_if_by_name(name);
  if (!netif)
    return -ENODEV;
  struct netdev *dev = net_if_dev(netif);
  uint32_t a;
  ip4_addr_t ip;

  switch (request) {
  case SIOCGIFINDEX:
    r->ivalue = netif_get_index(netif);
    return 0;
  case SIOCGIFFLAGS:
    r->flags = (short)net_if_flags(netif);
    return 0;
  case SIOCSIFFLAGS:
    if (r->flags & IFF_UP)
      netif_set_up(netif);
    else
      netif_set_down(netif);
    return 0;
  case SIOCGIFADDR:
  case SIOCGIFNETMASK:
  case SIOCGIFBRDADDR:
  case SIOCGIFDSTADDR: {
    uint32_t addr = ip4_addr_get_u32(netif_ip4_addr(netif));
    uint32_t mask = ip4_addr_get_u32(netif_ip4_netmask(netif));
    if (!addr)
      return -EADDRNOTAVAIL;
    if (request == SIOCGIFNETMASK)
      addr = mask;
    else if (request == SIOCGIFBRDADDR)
      addr = (addr & mask) | ~mask;
    put_in(&r->addr, addr);
    return 0;
  }
  case SIOCSIFADDR:
    if (get_in(&r->addr, &a))
      return -EINVAL;
    ip4_addr_set_u32(&ip, a);
    if (a) {
      ip4_addr_t mask;
      ip4_addr_set_u32(&mask, class_mask(a));
      netif_set_netmask(netif, &mask);
    }
    netif_set_ipaddr(netif, &ip);
    /* Linux brings an interface up when it gets an address. */
    netif_set_up(netif);
    return 0;
  case SIOCSIFNETMASK:
    if (get_in(&r->addr, &a))
      return -EINVAL;
    ip4_addr_set_u32(&ip, a);
    netif_set_netmask(netif, &ip);
    return 0;
  case SIOCSIFBRDADDR:
    return 0; /* always the network's broadcast address */
  case SIOCGIFMTU:
    r->ivalue = netif->mtu;
    return 0;
  case SIOCSIFMTU:
    if (r->ivalue < 68 || (dev && r->ivalue > dev->mtu))
      return -EINVAL;
    netif->mtu = (u16_t)r->ivalue;
    return 0;
  case SIOCGIFHWADDR:
    memset(&r->addr, 0, sizeof(r->addr));
    r->addr.family = dev ? ARPHRD_ETHER : ARPHRD_LOOPBACK;
    if (dev)
      memcpy(r->addr.data, dev->mac, 6);
    return 0;
  case SIOCGIFMETRIC:
    r->ivalue = 0;
    return 0;
  case SIOCSIFMETRIC:
    return 0;
  case SIOCGIFTXQLEN:
    r->ivalue = 1000;
    return 0;
  case SIOCSIFTXQLEN:
    return 0;
  case SIOCGIFMAP:
    memset(r->raw, 0, sizeof(r->raw));
    return 0;
  case SIOCSIFMAP:
    return -EOPNOTSUPP;
  }
  return -ENOTTY;
}

long net_if_ioctl(unsigned long request, void *arg) {
  long ret;
  net_lock();
  if (request == SIOCGIFCONF)
    ret = list_ifs(arg);
  else if (request == SIOCADDRT || request == SIOCDELRT)
    ret = route(request, arg);
  else if ((request & 0xff00) == 0x8900)
    ret = if_request(request, arg);
  else
    ret = -ENOTTY;
  net_unlock();
  return ret;
}
