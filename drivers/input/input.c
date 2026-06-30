/**
 * @file input.c
 * @brief Input core — device and handler registration, event dispatch
 */
#include <hubble/input.h>
#include <hubble/printk.h>
#include <smp/spinlock.h>

/* ── Global lists ───────────────────────────────────────── */

static input_dev_t *devices = NULL;
static input_handler_t *handlers = NULL;

/*
 * Both devices and handlers lists can be touched from IRQ context
 * (e.g. keyboard_irq calls input_report which walks handlers), so
 * all mutations are protected by irqlocks.
 */
static irqlock_t devices_lock = IRQLOCK_INIT("input_devices");
static irqlock_t handlers_lock = IRQLOCK_INIT("input_handlers");

/* ── Match a device against all registered handlers ─────── */

static void attach_device_to_handlers(input_dev_t *dev)
{
	irqlock_acquire(&handlers_lock);

	for (input_handler_t *h = handlers; h; h = h->next)
	{
		if (h->match && !h->match(h, dev))
			continue;

		irqlock_acquire(&devices_lock);
		if (h->connect)
			h->connect(h, dev);
		irqlock_release(&devices_lock);
	}

	irqlock_release(&handlers_lock);
}

/* ── Match a handler against all already-registered devices ─ */

static void attach_handler_to_devices(input_handler_t *handler)
{
	irqlock_acquire(&devices_lock);

	for (input_dev_t *dev = devices; dev; dev = dev->next)
	{
		if (handler->match && !handler->match(handler, dev))
			continue;
		if (handler->connect)
			handler->connect(handler, dev);
	}

	irqlock_release(&devices_lock);
}

/* ── Device registration ────────────────────────────────── */

int input_register_device(input_dev_t *dev)
{
	if (!dev || !dev->name)
		return -1;

	dev->handles = NULL;
	dev->next = NULL;

	irqlock_acquire(&devices_lock);
	dev->next = devices;
	devices = dev;
	irqlock_release(&devices_lock);

	printk(KERN_INFO "input: registered device '%s'\n", dev->name);

	attach_device_to_handlers(dev);

	return 0;
}

void input_unregister_device(input_dev_t *dev)
{
	if (!dev)
		return;

	irqlock_acquire(&devices_lock);

	input_handle_t *h = dev->handles;
	while (h)
	{
		input_handle_t *next = h->next;
		if (h->handler->disconnect)
			h->handler->disconnect(h);
		h = next;
	}
	dev->handles = NULL;

	input_dev_t **pp = &devices;
	while (*pp && *pp != dev)
		pp = &(*pp)->next;
	if (*pp)
		*pp = dev->next;

	irqlock_release(&devices_lock);

	printk(KERN_INFO "input: unregistered device '%s'\n", dev->name);
}

/* ── Handler registration ───────────────────────────────── */

int input_register_handler(input_handler_t *handler)
{
	if (!handler || !handler->name)
		return -1;

	handler->next = NULL;

	irqlock_acquire(&handlers_lock);
	handler->next = handlers;
	handlers = handler;
	irqlock_release(&handlers_lock);

	printk(KERN_INFO "input: registered handler '%s'\n", handler->name);

	attach_handler_to_devices(handler);

	return 0;
}

void input_unregister_handler(input_handler_t *handler)
{
	if (!handler)
		return;

	irqlock_acquire(&handlers_lock);

	input_handler_t **pp = &handlers;
	while (*pp && *pp != handler)
		pp = &(*pp)->next;
	if (*pp)
		*pp = handler->next;

	irqlock_release(&handlers_lock);

	irqlock_acquire(&devices_lock);

	for (input_dev_t *dev = devices; dev; dev = dev->next)
	{
		input_handle_t **hp = &dev->handles;
		while (*hp)
		{
			if ((*hp)->handler == handler)
			{
				input_handle_t *h = *hp;
				*hp = h->next;
				if (handler->disconnect)
					handler->disconnect(h);
			}
			else
			{
				hp = &(*hp)->next;
			}
		}
	}

	irqlock_release(&devices_lock);

	printk(KERN_INFO "input: unregistered handler '%s'\n", handler->name);
}

/* ── Handle link management ─────────────────────────────── */

void input_link_handle(input_handle_t *handle)
{
	if (!handle || !handle->dev)
		return;

	handle->next = handle->dev->handles;
	handle->dev->handles = handle;
}

void input_unlink_handle(input_handle_t *handle)
{
	if (!handle || !handle->dev)
		return;

	input_handle_t **hp = &handle->dev->handles;
	while (*hp && *hp != handle)
		hp = &(*hp)->next;
	if (*hp)
		*hp = handle->next;
}

/* ── Event dispatch ─────────────────────────────────────── */

void input_report(input_dev_t *dev, input_raw_event_t *event)
{
	if (!dev || !event)
		return;

	for (input_handle_t *h = dev->handles; h; h = h->next)
	{
		if (h->handler->event)
			h->handler->event(h, event);
	}
}
