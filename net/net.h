/*
 * Inside the network stack: the glue between lwIP (net/lwip), network
 * devices (include/hubble/netdev.h) and sockets (net/socket.c).
 *
 * lwIP runs without threads of its own (NO_SYS): every call into it, and
 * every lwIP callback, happens under the net lock. The net thread
 * (net/core.c) takes it to poll the cards and run lwIP's timers; socket
 * syscalls take it around their lwIP calls and drop it to sleep.
 */
#pragma once

#include <hubble/netdev.h>
#include <hubble/stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct netif;
struct pbuf;

void net_lock(void);
/** Also delivers what was sent to 127.0.0.1 meanwhile. */
void net_unlock(void);

/** @brief Start lwIP and the net thread (once, before any card or socket). */
void net_init(void);

/* -- Interfaces ---------------------------------------------------------- */

/** The interface called @p name (eth0, lo), or NULL. Net lock held. */
struct netif *net_if_by_name(const char *name);
/** The interface with index @p ifindex (lo is 1), or NULL. */
struct netif *net_if_by_index(int ifindex);
/** Its Linux name: lo, eth0, ... */
const char *net_if_name(struct netif *netif);
/** Its device, NULL for lo. */
struct netdev *net_if_dev(struct netif *netif);
/** Send a whole Ethernet frame on @p netif (packet sockets).
 * @return 0 or -errno */
/** SIOCGIFFLAGS's IFF_* flags of @p netif, and IFF_LOWER_UP. */
unsigned net_if_flags(struct netif *netif);

/**
 * Add or delete a route (route(8), ip route), net lock held. lwIP has no
 * routing table: the default route (@p dst and @p mask 0, via @p gw) is
 * the default interface's gateway, and a route to an interface's own
 * network exists with its address. @p netif may be NULL (any).
 * @return 0 or -errno (-EOPNOTSUPP for any other route)
 */
long net_route_change(bool add, uint32_t dst, uint32_t mask, uint32_t gw,
                      struct netif *netif);

/** Queues one reply datagram for a netlink socket. @return 0 or -errno */
typedef long (*net_nl_reply_fn)(void *ctx, const void *data, size_t len);

/**
 * Handle the rtnetlink messages in @p buf (net/netlink.c), net lock held;
 * replies, addressed to port @p portid, go through @p reply.
 * @return 0, or the error of a reply that could not be queued
 */
long net_rtnl_request(const void *buf, size_t len, uint32_t portid,
                      net_nl_reply_fn reply, void *ctx);

/* /proc/net text: appended to a buffer, snprintf style (what does not
 * fit is cut). */
typedef struct {
  char *buf;
  size_t size, len;
} proc_out_t;

#define PROC_PRINT(o, ...)                                                     \
  do {                                                                         \
    if ((o)->len < (o)->size)                                                  \
      (o)->len += (size_t)snprintf((o)->buf + (o)->len, (o)->size - (o)->len,  \
                                   __VA_ARGS__);                               \
  } while (0)

/** The length written (without the NUL). */
static inline size_t proc_len(const proc_out_t *o) {
  return o->len < o->size ? o->len : (o->size ? o->size - 1 : 0);
}

/** /proc/net/dev and /proc/net/route as Linux writes them, into @p buf
 * (cut to @p size, NUL-terminated). @return the length */
size_t net_proc_dev(char *buf, size_t size);
size_t net_proc_route(char *buf, size_t size);
/** /proc/net/tcp, udp and raw (net/socket.c). @p v6: the IPv6 tables,
 * which only have the header (there is no IPv6). */
size_t net_proc_tcp(char *buf, size_t size, bool v6);
size_t net_proc_udp(char *buf, size_t size, bool v6);
size_t net_proc_raw(char *buf, size_t size, bool v6);

int net_if_xmit(struct netif *netif, const void *frame, size_t len);

/* -- Packet sockets ------------------------------------------------------ */

/** Every frame received or sent goes here first (net lock held). */
void packet_tap(struct netif *netif, const uint8_t *frame, size_t len,
                bool outgoing);
