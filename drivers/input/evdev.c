/**
 * @file evdev.c
 * @brief Event device — the events of every input device, in the Linux
 *        struct input_event format, readable from /dev/input/events
 *
 * Opening it discards events queued earlier; reads block until at least
 * one event is queued and return whole events only; poll() reports
 * readability. All devices share one queue,
 * so a graphical program gets keyboard and mouse input from one file.
 * Keyboard scancodes are translated to Linux key codes here, at the user
 * boundary; everything else passes through unchanged.
 */
#include <fs/vfs/dev.h>
#include <hpet/hpet.h>
#include <hubble/input.h>
#include <hubble/module.h>
#include <hubble/printk.h>
#include <hubble/string.h>
#include <mm/kmalloc.h>
#include <smp/spinlock.h>
#include <smp/waitqueue.h>
#include <stdbool.h>

#define EVDEV_NAME "input/events"
#define EVDEV_QUEUE_SIZE 256 /* events; a power of two */
#define EVDEV_READ_CHUNK 16

/** Linux struct input_event on x86_64 */
struct evdev_event {
  int64_t tv_sec;
  int64_t tv_usec;
  uint16_t type;
  uint16_t code;
  int32_t value;
};

static struct evdev_event queue[EVDEV_QUEUE_SIZE];
static uint32_t queue_head; /* next slot to write */
static uint32_t queue_tail; /* next slot to read */
static irqlock_t queue_lock = IRQLOCK_INIT("evdev");
static wait_queue_t read_wq;

/* -- Key codes -------------------------------------------- */

/**
 * Linux key codes equal set-1 scancodes for the main block; E0-prefixed
 * keys get their own codes. Returns 0 for keys without one.
 */
static uint16_t evdev_key_code(uint16_t code) {
  if (!(code & 0x100))
    return code;

  switch (code & 0xFF) {
  case 0x1C:
    return 96; /* KEY_KPENTER */
  case 0x1D:
    return 97; /* KEY_RIGHTCTRL */
  case 0x35:
    return 98; /* KEY_KPSLASH */
  case 0x37:
    return 99; /* KEY_SYSRQ */
  case 0x38:
    return 100; /* KEY_RIGHTALT */
  case 0x47:
    return 102; /* KEY_HOME */
  case 0x48:
    return 103; /* KEY_UP */
  case 0x49:
    return 104; /* KEY_PAGEUP */
  case 0x4B:
    return 105; /* KEY_LEFT */
  case 0x4D:
    return 106; /* KEY_RIGHT */
  case 0x4F:
    return 107; /* KEY_END */
  case 0x50:
    return 108; /* KEY_DOWN */
  case 0x51:
    return 109; /* KEY_PAGEDOWN */
  case 0x52:
    return 110; /* KEY_INSERT */
  case 0x53:
    return 111; /* KEY_DELETE */
  case 0x5B:
    return 125; /* KEY_LEFTMETA */
  case 0x5C:
    return 126; /* KEY_RIGHTMETA */
  case 0x5D:
    return 127; /* KEY_COMPOSE */
  default:
    return 0;
  }
}

/* -- Input handler ---------------------------------------- */

static bool evdev_match(input_handler_t *handler, input_dev_t *dev) {
  (void)handler;
  (void)dev;
  return true;
}

static int evdev_connect(input_handler_t *handler, input_dev_t *dev) {
  input_handle_t *handle = kzalloc(sizeof(*handle));
  if (!handle)
    return -1;

  handle->dev = dev;
  handle->handler = handler;
  input_link_handle(handle);
  return 0;
}

static void evdev_disconnect(input_handle_t *handle) {
  input_unlink_handle(handle);
  kfree(handle);
}

/* Runs in IRQ context for PS/2 devices. */
static void evdev_event(input_handle_t *handle, input_raw_event_t *ev) {
  uint16_t code = ev->code;
  bool keyboard = !input_test_bit(EV_REL, handle->dev->evbit);

  if (ev->type == EV_KEY && keyboard) {
    code = evdev_key_code(code);
    if (!code)
      return;
  }

  uint64_t now = hpet_get_time_ns();
  struct evdev_event out = {
      .tv_sec = (int64_t)(now / 1000000000ULL),
      .tv_usec = (int64_t)(now % 1000000000ULL / 1000),
      .type = ev->type,
      .code = code,
      .value = ev->value,
  };

  irqlock_acquire(&queue_lock);
  /* Full: drop the event rather than overwrite unread ones. */
  bool queued = queue_head - queue_tail < EVDEV_QUEUE_SIZE;
  if (queued)
    queue[queue_head++ % EVDEV_QUEUE_SIZE] = out;
  irqlock_release(&queue_lock);

  if (queued)
    waitqueue_wake_all(&read_wq);
}

static input_handler_t evdev_handler = {
    .name = "evdev",
    .match = evdev_match,
    .connect = evdev_connect,
    .disconnect = evdev_disconnect,
    .event = evdev_event,
};

/* -- Device file ------------------------------------------ */

static bool evdev_readable(void) { return queue_head != queue_tail; }

static bool evdev_readable_cond(void *arg) {
  (void)arg;
  return evdev_readable();
}

static uint64_t evdev_read(uint64_t offset, size_t size, void *buf) {
  (void)offset;
  size_t want = size / sizeof(struct evdev_event);
  if (!want)
    return 0;

  waitqueue_wait_event(&read_wq, evdev_readable_cond, NULL, 0);

  /* Copy out in chunks: the user buffer may fault, which must not
   * happen with the queue lock (and interrupts) held. */
  size_t done = 0;
  while (done < want) {
    struct evdev_event chunk[EVDEV_READ_CHUNK];
    size_t n = 0;

    irqlock_acquire(&queue_lock);
    while (n < EVDEV_READ_CHUNK && done + n < want && queue_tail != queue_head)
      chunk[n++] = queue[queue_tail++ % EVDEV_QUEUE_SIZE];
    irqlock_release(&queue_lock);

    if (!n)
      break;
    memcpy((uint8_t *)buf + done * sizeof(struct evdev_event), chunk,
           n * sizeof(struct evdev_event));
    done += n;
  }

  return done * sizeof(struct evdev_event);
}

/* A reader wants events from now on, not what queued up before it. */
static void evdev_open(void) {
  irqlock_acquire(&queue_lock);
  queue_tail = queue_head;
  irqlock_release(&queue_lock);
}

static int evdev_init(void) {
  waitqueue_init(&read_wq);
  dev_vfs_register(EVDEV_NAME, NULL, evdev_read, NULL);
  dev_vfs_set_char_ops(EVDEV_NAME, evdev_open, NULL, evdev_readable,
                       (struct wait_queue *)&read_wq);
  return input_register_handler(&evdev_handler);
}

module_init(evdev_init);
MODULE_NAME("evdev");
