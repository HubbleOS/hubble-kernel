#pragma once

/**
 * @brief Network devices: what a network card driver gives the stack.
 *
 * A driver fills in a struct netdev (MAC, MTU, ops) and calls
 * netdev_register(), which names it eth0, eth1, ... and attaches it to
 * the TCP/IP stack (net/). The stack's net thread then calls ops->poll
 * every few milliseconds, and at once after netdev_wake() (which a
 * driver calls from its interrupt handler); the driver hands each
 * received frame to netdev_rx(). Frames go out through ops->xmit.
 *
 * Every op runs with the net lock held and interrupts off, so it must
 * not sleep or take long: xmit queues the frame and returns.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NETDEV_NAME_MAX 16
#define NETDEV_FRAME_MAX 1518 /* Ethernet, with header, without FCS */

struct netdev;

struct netdev_ops {
  /** Queue one Ethernet frame (header included, no FCS) to send.
   * @return 0, or -1 if the card has no room (the frame is dropped) */
  int (*xmit)(struct netdev *dev, const void *frame, size_t len);
  /** Pass every frame received since the last call to netdev_rx(). */
  void (*poll)(struct netdev *dev);
  /** Whether the link (cable, or the virtual one) is up. */
  bool (*link_up)(struct netdev *dev);
};

struct netdev {
  /* Set by the driver before netdev_register(). */
  uint8_t mac[6];
  uint16_t mtu;
  const struct netdev_ops *ops;
  void *priv; /**< The driver's */

  /* Set by netdev_register(). */
  char name[NETDEV_NAME_MAX];
  int ifindex;
  void *stack; /**< The stack's per-interface state */
};

/** @brief Attach @p dev (which must stay allocated) to the stack.
 * @return 0, or -errno */
int netdev_register(struct netdev *dev);

/** @brief Hand a received frame to the stack (from ops->poll). The data is
 * copied; @p frame may be reused once this returns. */
void netdev_rx(struct netdev *dev, const void *frame, size_t len);

/** @brief Frames arrived (or the link changed): poll now rather than at
 * the next tick. Safe from an interrupt handler. */
void netdev_wake(struct netdev *dev);
