#pragma once

/**
 * @brief Input subsystem — device, handler, and event abstractions.
 *
 * Mirrors the architecture of the Linux input layer:
 * - input_dev represents a physical input device (keyboard, mouse, …)
 * - input_handler consumes events (TTY, evdev, …)
 * - input_handle connects one dev to one handler
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* -- Bitmap helpers ---------------------------------------------------------
 */

#define BITS_PER_LONG (sizeof(unsigned long) * 8)
#define BITS_TO_LONGS(n) (((n) + BITS_PER_LONG - 1) / BITS_PER_LONG)

#define input_set_bit(bit, arr)                                                \
  ((arr)[(bit) / BITS_PER_LONG] |= (1UL << ((bit) % BITS_PER_LONG)))
#define input_test_bit(bit, arr)                                               \
  ((arr)[(bit) / BITS_PER_LONG] & (1UL << ((bit) % BITS_PER_LONG)))
#define input_clear_bit(bit, arr)                                              \
  ((arr)[(bit) / BITS_PER_LONG] &= ~(1UL << ((bit) % BITS_PER_LONG)))

/* -- Event types and codes --------------------------------------------------
 *
 * Event types and relative axes use the Linux numbering, so evdev hands
 * them to user space unchanged. Key codes depend on the device:
 *   - keyboards report EV_KEY with PS/2 set-1 scancodes, 0x100 marking an
 *     E0-prefixed key (what the tty keymap decodes);
 *   - pointing devices (those with EV_REL) report buttons as BTN_*.
 * A driver ends each batch of related events with input_sync().
 */

#define EV_SYN 0x00 /**< Separates batches of events.           */
#define EV_KEY 0x01 /**< Keys / buttons.                        */
#define EV_REL 0x02 /**< Relative motion (mouse, scroll).       */
#define EV_ABS 0x03 /**< Absolute coordinates (touchscreen).    */
#define EV_CNT 0x04 /**< Number of event types — must be last.  */

#define SYN_REPORT 0

/* -- Key code bitmap size ---------------------------------------------------
 */

#define KEY_CNT 256

#define BTN_LEFT 0x110
#define BTN_RIGHT 0x111
#define BTN_MIDDLE 0x112

/* -- Relative axis identifiers (relbit) -------------------------------------
 */

#define REL_X 0x00
#define REL_Y 0x01
#define REL_WHEEL 0x08
#define REL_CNT 0x10

/* -- Input event ------------------------------------------------------------
 */

typedef struct {
  uint16_t type; /**< One of EV_*.                          */
  uint16_t code; /**< Sub-code (KEY_A, REL_X, …).          */
  int32_t value; /**< Press(1)/release(0)/repeat(2), delta, absolute. */
} input_raw_event_t;

/* -- Forward declarations ---------------------------------------------------
 */

struct input_dev;
struct input_handler;
struct input_handle;

/**
 * @brief Physical input device.
 *
 * Drivers (PS/2, USB HID, …) fill in the fields and register the device.
 */
typedef struct input_dev {
  const char *name;
  unsigned long evbit[BITS_TO_LONGS(EV_CNT)];
  unsigned long keybit[BITS_TO_LONGS(KEY_CNT)];
  unsigned long relbit[BITS_TO_LONGS(REL_CNT)];
  struct input_handle *handles;
  struct input_dev *next;
} input_dev_t;

/**
 * @brief Input event consumer.
 *
 * Registered once by the consumer (TTY, evdev, …) and connected to every
 * matching device by the core.
 */
typedef struct input_handler {
  const char *name;
  bool (*match)(struct input_handler *handler, input_dev_t *dev);
  int (*connect)(struct input_handler *handler, input_dev_t *dev);
  void (*disconnect)(struct input_handle *handle);
  void (*event)(struct input_handle *handle, input_raw_event_t *event);
  struct input_handler *next;
} input_handler_t;

/**
 * @brief Connection between one device and one handler.
 *
 * Created by handler->connect(), linked by input_link_handle().
 */
typedef struct input_handle {
  void *private;
  input_dev_t *dev;
  input_handler_t *handler;
  struct input_handle *next;
} input_handle_t;

/* -- Core API ---------------------------------------------------------------
 */

int input_register_device(input_dev_t *dev);
void input_unregister_device(input_dev_t *dev);

int input_register_handler(input_handler_t *handler);
void input_unregister_handler(input_handler_t *handler);

void input_link_handle(input_handle_t *handle);
void input_unlink_handle(input_handle_t *handle);

void input_report(input_dev_t *dev, input_raw_event_t *event);

/** Report one event: input_report() without building the struct. */
static inline void input_event(input_dev_t *dev, uint16_t type, uint16_t code,
                               int32_t value) {
  input_raw_event_t ev = {.type = type, .code = code, .value = value};
  input_report(dev, &ev);
}

/** End a batch of events (one mouse packet, one keyboard report). */
static inline void input_sync(input_dev_t *dev) {
  input_event(dev, EV_SYN, SYN_REPORT, 0);
}
