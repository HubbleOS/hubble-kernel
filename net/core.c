/*
 * The network stack's core: the net lock, network devices as lwIP
 * interfaces, and the net thread (see net.h).
 *
 * Cards are polled, like the xHCI controller: the net thread asks each
 * one for received frames every NET_POLL_NS and runs lwIP's timers.
 * Sending needs no thread: lwIP calls the card's xmit directly from the
 * syscall that sends.
 */
#include "net.h"

#include "lwip/etharp.h"
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/timeouts.h"
#include "netif/ethernet.h"

#include <hpet/hpet.h>
#include <hubble/errno.h>
#include <hubble/printk.h>
#include <hubble/stdio.h>
#include <hubble/string.h>
#include <mm/kmalloc.h>
#include <smp/scheduler.h>
#include <smp/spinlock.h>

#define NET_MAX_IFS 8
#define NET_POLL_NS 2000000ULL /* 2 ms; a tick at 100 Hz in practice */
#define NET_THREAD_PRIORITY 100

typedef struct {
  struct netif netif; /* first: lwIP's state pointer leads back here */
  struct netdev *dev;
  bool link;
  /* For /proc/net/dev, under the net lock. */
  uint64_t rx_bytes, rx_packets, rx_dropped;
  uint64_t tx_bytes, tx_packets, tx_errors;
} net_if_t;

static irqlock_t lock = IRQLOCK_INIT("net");
static bool started;
static task_t *thread;
static net_if_t *ifs[NET_MAX_IFS];
static int nifs;

void net_lock(void) { irqlock_acquire(&lock); }

void net_unlock(void) {
  /* With NO_SYS, packets to lo wait in a queue until polled: deliver them
   * now, so 127.0.0.1 does not wait for the net thread. */
  netif_poll_all();
  irqlock_release(&lock);
}

/* -- Interfaces ---------------------------------------------------------- */

struct netdev *net_if_dev(struct netif *netif) {
  net_if_t *nif = netif->state;
  return nif ? nif->dev : NULL;
}

const char *net_if_name(struct netif *netif) {
  struct netdev *dev = net_if_dev(netif);
  return dev ? dev->name : "lo"; /* lwIP's loopback has no state */
}

struct netif *net_if_by_name(const char *name) {
  struct netif *netif;
  NETIF_FOREACH(netif) {
    if (!strcmp(net_if_name(netif), name))
      return netif;
  }
  return NULL;
}

struct netif *net_if_by_index(int ifindex) {
  if (ifindex <= 0 || ifindex > 255)
    return NULL;
  return netif_get_by_index((u8_t)ifindex);
}

/* Every frame out of a card goes through here: packet sockets see it,
 * and it is counted. */
static int if_send(struct netif *netif, const void *frame, size_t len) {
  net_if_t *nif = netif->state;
  packet_tap(netif, frame, len, true);
  if (nif->dev->ops->xmit(nif->dev, frame, len)) {
    nif->tx_errors++;
    return -ENOBUFS;
  }
  nif->tx_packets++;
  nif->tx_bytes += len;
  return 0;
}

int net_if_xmit(struct netif *netif, const void *frame, size_t len) {
  if (!net_if_dev(netif))
    return -EOPNOTSUPP; /* lo carries IP packets, not frames */
  if (!netif_is_up(netif))
    return -ENETDOWN;
  if (len < 14 || len > NETDEV_FRAME_MAX)
    return -EMSGSIZE;
  return if_send(netif, frame, len);
}

/* lwIP sends a frame: copy the pbuf chain into one buffer for the card. */
static err_t if_linkoutput(struct netif *netif, struct pbuf *p) {
  static uint8_t frame[NETDEV_FRAME_MAX]; /* under the net lock */
  if (p->tot_len > sizeof(frame))
    return ERR_BUF;
  pbuf_copy_partial(p, frame, p->tot_len, 0);
  return if_send(netif, frame, p->tot_len) ? ERR_IF : ERR_OK;
}

static err_t if_init(struct netif *netif) {
  struct netdev *dev = net_if_dev(netif);
  netif->name[0] = 'e';
  netif->name[1] = 't';
  netif->output = etharp_output;
  netif->linkoutput = if_linkoutput;
  netif->mtu = dev->mtu;
  netif->hwaddr_len = ETH_HWADDR_LEN;
  memcpy(netif->hwaddr, dev->mac, ETH_HWADDR_LEN);
  netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_ETHERNET;
  return ERR_OK;
}

int netdev_register(struct netdev *dev) {
  if (!started)
    return -ENODEV;
  net_if_t *nif = kzalloc(sizeof(*nif));
  if (!nif)
    return -ENOMEM;

  net_lock();
  if (nifs == NET_MAX_IFS) {
    net_unlock();
    kfree(nif);
    return -ENOSPC;
  }
  snprintf(dev->name, sizeof(dev->name), "eth%d", nifs);
  if (!dev->mtu)
    dev->mtu = 1500;
  nif->dev = dev;
  dev->stack = nif;
  /* Down and without an address, as on Linux: ifconfig or udhcpc set it
   * up. The first card is where traffic for other networks goes. */
  netif_add_noaddr(&nif->netif, nif, if_init, ethernet_input);
  /* lwIP puts a new interface first; move it last, so that every walk of
   * the list (ip, ifconfig, /proc/net/dev) goes in ifindex order. */
  netif_list = nif->netif.next;
  struct netif **tail = &netif_list;
  while (*tail)
    tail = &(*tail)->next;
  *tail = &nif->netif;
  nif->netif.next = NULL;
  dev->ifindex = netif_get_index(&nif->netif);
  if (!netif_default)
    netif_set_default(&nif->netif);
  ifs[nifs++] = nif;
  net_unlock();

  printk(KERN_INFO "net: %s: MAC %02x:%02x:%02x:%02x:%02x:%02x\n", dev->name,
         dev->mac[0], dev->mac[1], dev->mac[2], dev->mac[3], dev->mac[4],
         dev->mac[5]);
  return 0;
}

void netdev_rx(struct netdev *dev, const void *frame, size_t len) {
  net_if_t *nif = dev->stack;
  if (len < 14 || len > NETDEV_FRAME_MAX || !netif_is_up(&nif->netif))
    return; /* a down interface receives nothing */
  nif->rx_packets++;
  nif->rx_bytes += len;
  packet_tap(&nif->netif, frame, len, false);

  struct pbuf *p = pbuf_alloc(PBUF_RAW, (u16_t)len, PBUF_POOL);
  if (!p) {
    nif->rx_dropped++; /* out of buffers, as a busy card would */
    return;
  }
  pbuf_take(p, frame, (u16_t)len);
  if (nif->netif.input(p, &nif->netif) != ERR_OK)
    pbuf_free(p);
}

/* -- /proc/net ----------------------------------------------------------- */

size_t net_proc_dev(char *buf, size_t size) {
  proc_out_t o = {buf, size, 0};
  PROC_PRINT(&o, "Inter-|   Receive                            "
                 "                    |  Transmit\n"
                 " face |bytes    packets errs drop fifo frame compressed "
                 "multicast|bytes    packets errs drop fifo colls carrier "
                 "compressed\n");
  net_lock();
  struct netif *netif;
  NETIF_FOREACH(netif) {
    net_if_t *nif = netif->state;
    uint64_t z = 0;
    PROC_PRINT(&o,
               "%6s: %7lu %7lu %4lu %4lu    0     0          0         0 "
               "%8lu %7lu %4lu    0    0     0       0          0\n",
               net_if_name(netif), nif ? nif->rx_bytes : z,
               nif ? nif->rx_packets : z, z, nif ? nif->rx_dropped : z,
               nif ? nif->tx_bytes : z, nif ? nif->tx_packets : z,
               nif ? nif->tx_errors : z);
  }
  net_unlock();
  return proc_len(&o);
}

/* Linux prints addresses as the raw network-order word in hex. */
size_t net_proc_route(char *buf, size_t size) {
  proc_out_t o = {buf, size, 0};
  PROC_PRINT(&o, "Iface\tDestination\tGateway \tFlags\tRefCnt\tUse\t"
                 "Metric\tMask\t\tMTU\tWindow\tIRTT\n");
  net_lock();
  struct netif *netif;
  /* The default route first, then each interface's own network. */
  if (netif_default && netif_is_up(netif_default) &&
      !ip4_addr_isany_val(*netif_ip4_gw(netif_default)))
    PROC_PRINT(&o, "%s\t00000000\t%08X\t0003\t0\t0\t0\t00000000\t0\t0\t0\n",
               net_if_name(netif_default),
               ip4_addr_get_u32(netif_ip4_gw(netif_default)));
  NETIF_FOREACH(netif) {
    uint32_t a = ip4_addr_get_u32(netif_ip4_addr(netif));
    uint32_t m = ip4_addr_get_u32(netif_ip4_netmask(netif));
    if (!net_if_dev(netif) || !a || !netif_is_up(netif))
      continue; /* Linux lists lo's route in another table */
    PROC_PRINT(&o, "%s\t%08X\t00000000\t0001\t0\t0\t0\t%08X\t0\t0\t0\n",
               net_if_name(netif), a & m, m);
  }
  net_unlock();
  return proc_len(&o);
}

/* -- The net thread ------------------------------------------------------ */

void netdev_wake(struct netdev *dev) {
  (void)dev; /* one thread polls every card */
  if (!thread)
    return;
  task_wake(thread);
  scheduler_kick(thread); /* another CPU: an IPI */
  need_resched = true;    /* this one: on the way out of the interrupt */
}

static void net_thread(void *arg) {
  (void)arg;
  for (;;) {
    /* Blocked from here: a netdev_wake() while we poll makes the
     * task_wait() below return at once, so no frame waits for a tick. */
    task_prepare_wait();
    net_lock();
    for (int i = 0; i < nifs; i++) {
      net_if_t *nif = ifs[i];
      struct netdev *dev = nif->dev;
      dev->ops->poll(dev);

      bool link = dev->ops->link_up(dev);
      if (link != nif->link) {
        nif->link = link;
        if (link)
          netif_set_link_up(&nif->netif);
        else
          netif_set_link_down(&nif->netif);
        printk(KERN_INFO "net: %s: link %s\n", dev->name, link ? "up" : "down");
      }
    }
    sys_check_timeouts();
    net_unlock();

    task_wait(hpet_get_time_ns() + NET_POLL_NS);
  }
}

void net_init(void) {
  if (started)
    return;
  lwip_init(); /* also brings up lo, 127.0.0.1 */
  /* lwIP leaves lo's MTU at 0 (no limit); report Linux's 65536, as far as
   * the 16-bit field goes. */
  struct netif *lo;
  NETIF_FOREACH(lo) {
    if (!net_if_dev(lo))
      lo->mtu = 65535;
  }
  started = true;
  thread = _task_create_with_arg(net_thread, NULL, NET_THREAD_PRIORITY, false);
  if (!thread) {
    printk(KERN_ERR "net: failed to start the net thread\n");
    return;
  }
  scheduler_add_task(thread);
  printk(KERN_OK "net: TCP/IP stack (lwIP %s) started\n", LWIP_VERSION_STRING);
}
