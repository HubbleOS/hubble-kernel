#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Bitmap helpers ──────────────────────────────────────────────────────── */

#define BITS_PER_LONG (sizeof(unsigned long) * 8)
#define BITS_TO_LONGS(n) (((n) + BITS_PER_LONG - 1) / BITS_PER_LONG)

#define input_set_bit(bit, arr) ((arr)[(bit) / BITS_PER_LONG] |= (1UL << ((bit) % BITS_PER_LONG)))
#define input_test_bit(bit, arr) ((arr)[(bit) / BITS_PER_LONG] & (1UL << ((bit) % BITS_PER_LONG)))
#define input_clear_bit(bit, arr) ((arr)[(bit) / BITS_PER_LONG] &= ~(1UL << ((bit) % BITS_PER_LONG)))

/* ── Event types (evbit) ─────────────────────────────────────────────────── */
/*
 * Що пристрій вміє генерувати.
 * Один пристрій може мати кілька типів одночасно.
 */
#define EV_KEY 0 /* клавіші / кнопки                  */
#define EV_REL 1 /* відносний рух (миша, скрол)       */
#define EV_ABS 2 /* абсолютні координати (touchscreen) */
#define EV_CNT 3 /* кількість типів — завжди останнє  */

/* ── Key codes (keybit) ──────────────────────────────────────────────────── */
/*
 * Які конкретно клавіші/кнопки пристрій підтримує.
 * Використовується тільки якщо evbit має EV_KEY.
 */
#define KEY_CNT 256

/* ── Relative axes (relbit) ──────────────────────────────────────────────── */
/*
 * Які відносні осі пристрій підтримує.
 * Використовується тільки якщо evbit має EV_REL.
 */
#define REL_X 0
#define REL_Y 1
#define REL_WHEEL 2
#define REL_CNT 3

/* ── Input event ─────────────────────────────────────────────────────────── */
/*
 * Одна подія яку пристрій відправляє в input core.
 *
 * type  — один з EV_*
 * code  — що саме: KEY_A, REL_X, ...
 * value — для EV_KEY: 1=press, 0=release, 2=repeat
 *         для EV_REL: delta
 *         для EV_ABS: абсолютна координата
 */
typedef struct
{
	uint16_t type;
	uint16_t code;
	int32_t value;
} input_raw_event_t;

/* ── Forward declarations ────────────────────────────────────────────────── */
struct input_dev;
struct input_handler;
struct input_handle;

/* ── input_dev ───────────────────────────────────────────────────────────── */
/*
 * Абстракція фізичного пристрою.
 * Драйвер (ps2, usb hid, ...) заповнює і реєструє.
 */
typedef struct input_dev
{
	const char *name;

	/* що вміє пристрій */
	unsigned long evbit[BITS_TO_LONGS(EV_CNT)];
	unsigned long keybit[BITS_TO_LONGS(KEY_CNT)];
	unsigned long relbit[BITS_TO_LONGS(REL_CNT)];

	/* список handle-ів підключених до цього dev (core manages) */
	struct input_handle *handles;

	/* для linked list всіх зареєстрованих пристроїв (core manages) */
	struct input_dev *next;
} input_dev_t;

/* ── input_handler ───────────────────────────────────────────────────────── */
/*
 * Споживач подій: tty kbd, evdev, debug console, ...
 * Реєструється один раз, підключається до кожного підходящого dev.
 */
typedef struct input_handler
{
	const char *name;

	/*
	 * Чи цікавить цей handler даний пристрій?
	 * Повертає true — core викличе connect().
	 */
	bool (*match)(struct input_handler *handler, input_dev_t *dev);

	/*
	 * Core викликає коли знайдено відповідний пристрій.
	 * Handler має створити input_handle і викликати input_link_handle().
	 */
	int (*connect)(struct input_handler *handler, input_dev_t *dev);
	void (*disconnect)(struct input_handle *handle);

	/* Власне обробка події */
	void (*event)(struct input_handle *handle, input_raw_event_t *event);

	/* для linked list всіх зареєстрованих handlers (core manages) */
	struct input_handler *next;
} input_handler_t;

/* ── input_handle ────────────────────────────────────────────────────────── */
/*
 * З'єднання конкретного dev з конкретним handler.
 * Handler створює в connect(), core додає через input_link_handle().
 *
 * Дозволяє: 1 handler → N devices, 1 device → N handlers.
 */
typedef struct input_handle
{
	void *private; /* handler може зберігати свій стан */

	input_dev_t *dev;
	input_handler_t *handler;

	/* linked list на стороні dev */
	struct input_handle *next;
} input_handle_t;

/* ── Core API ────────────────────────────────────────────────────────────── */

/* Реєстрація пристрою — викликається драйвером пристрою */
int input_register_device(input_dev_t *dev);
void input_unregister_device(input_dev_t *dev);

/* Реєстрація handler-а — викликається споживачем (tty, evdev, ...) */
int input_register_handler(input_handler_t *handler);
void input_unregister_handler(input_handler_t *handler);

/* Викликається з connect() handler-а після створення handle */
void input_link_handle(input_handle_t *handle);
void input_unlink_handle(input_handle_t *handle);

/* Відправити подію — викликається драйвером пристрою з IRQ */
void input_report(input_dev_t *dev, input_raw_event_t *event);
