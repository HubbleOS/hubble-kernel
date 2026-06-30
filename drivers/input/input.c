#include <hubble/input.h>
#include <hubble/printk.h>
#include <smp/spinlock.h>

/* ── Global lists ────────────────────────────────────────────────────────── */

static input_dev_t *devices = NULL;
static input_handler_t *handlers = NULL;

/*
 * irqlock — бо і devices і handlers можуть чіпатись з IRQ контексту
 * (keyboard_irq викликає input_report → ходить по handles)
 */
static irqlock_t devices_lock = IRQLOCK_INIT("input_devices");
static irqlock_t handlers_lock = IRQLOCK_INIT("input_handlers");

/* ── Internal: match dev з усіма handlers ────────────────────────────────── */

static void attach_device_to_handlers(input_dev_t *dev)
{
	irqlock_acquire(&handlers_lock);

	for (input_handler_t *h = handlers; h; h = h->next)
	{
		if (h->match && !h->match(h, dev))
			continue;
		/* connect() сам створює handle і викликає input_link_handle(),
		 * який вимагає devices_lock від callера */
		irqlock_acquire(&devices_lock);
		if (h->connect)
			h->connect(h, dev);
		irqlock_release(&devices_lock);
	}

	irqlock_release(&handlers_lock);
}

/* ── Internal: match handler з усіма вже зареєстрованими devices ─────────── */

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

/* ── input_register_device ───────────────────────────────────────────────── */

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

	/* підключаємо до вже існуючих handlers */
	attach_device_to_handlers(dev);

	return 0;
}

/* ── input_unregister_device ─────────────────────────────────────────────── */

void input_unregister_device(input_dev_t *dev)
{
	if (!dev)
		return;

	/* відключаємо всі handles */
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

	/* видаляємо з глобального списку */
	input_dev_t **pp = &devices;
	while (*pp && *pp != dev)
		pp = &(*pp)->next;
	if (*pp)
		*pp = dev->next;

	irqlock_release(&devices_lock);

	printk(KERN_INFO "input: unregistered device '%s'\n", dev->name);
}

/* ── input_register_handler ──────────────────────────────────────────────── */

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

	/* підключаємо до вже існуючих devices */
	attach_handler_to_devices(handler);

	return 0;
}

/* ── input_unregister_handler ────────────────────────────────────────────── */

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

	/*
	 * Відключаємо всі handles цього handler-а від їх devices.
	 * Йдемо по devices і шукаємо handles що належать цьому handler-у.
	 */
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

/* ── input_link_handle ───────────────────────────────────────────────────── */
/*
 * Викликається з connect() після того як handler виділив input_handle_t.
 * Додає handle в список dev->handles.
 */
void input_link_handle(input_handle_t *handle)
{
	if (!handle || !handle->dev)
		return;

	/* Caller MUST hold devices_lock (e.g. from attach_handler_to_devices
	 * or attach_device_to_handlers which already acquired it) */
	handle->next = handle->dev->handles;
	handle->dev->handles = handle;
}

void input_unlink_handle(input_handle_t *handle)
{
	if (!handle || !handle->dev)
		return;

	/* Caller MUST hold devices_lock */
	input_handle_t **hp = &handle->dev->handles;
	while (*hp && *hp != handle)
		hp = &(*hp)->next;
	if (*hp)
		*hp = handle->next;
}

/* ── input_report ────────────────────────────────────────────────────────── */
/*
 * Викликається драйвером пристрою (з IRQ або звичайного контексту).
 * Розсилає подію всім підключеним handlers.
 */
void input_report(input_dev_t *dev, input_raw_event_t *event)
{
	if (!dev || !event)
		return;

	/*
	 * Не беремо devices_lock тут навмисно —
	 * handles список міняється тільки при register/unregister,
	 * а під час роботи системи він стабільний.
	 *
	 * Якщо в майбутньому з'явиться гаряче відключення USB —
	 * треба буде додати refcount або RCU.
	 */
	for (input_handle_t *h = dev->handles; h; h = h->next)
	{
		if (h->handler->event)
			h->handler->event(h, event);
	}
}
