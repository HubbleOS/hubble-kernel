#include "gdt/interrupt.h"
#include <syscalls/syscall.h>
#include <sys/syscall.h>
#include <stdint.h>
#include "printk.h"

// ============================================================================
// PIC функции (без изменений)
// ============================================================================

#define PIC1_COMMAND 0x20
#define PIC1_DATA 0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA 0xA1
#define PIC_EOI 0x20

static inline void outb(uint16_t port, uint8_t value)
{
	asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
	uint8_t ret;
	asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
	return ret;
}

void pic_remap(void)
{
	uint8_t a1, a2;
	a1 = inb(PIC1_DATA);
	a2 = inb(PIC2_DATA);

	outb(PIC1_COMMAND, 0x11);
	outb(PIC2_COMMAND, 0x11);
	outb(PIC1_DATA, 32);
	outb(PIC2_DATA, 40);
	outb(PIC1_DATA, 4);
	outb(PIC2_DATA, 2);
	outb(PIC1_DATA, 0x01);
	outb(PIC2_DATA, 0x01);
	outb(PIC1_DATA, a1);
	outb(PIC2_DATA, a2);
}

void pic_send_eoi(uint8_t irq)
{
	if (irq >= 8)
		outb(PIC2_COMMAND, PIC_EOI);
	outb(PIC1_COMMAND, PIC_EOI);
}

void irq_set_mask(uint8_t irq)
{
	uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
	if (irq >= 8)
		irq -= 8;
	uint8_t value = inb(port) | (1 << irq);
	outb(port, value);
}

void irq_clear_mask(uint8_t irq)
{
	uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
	if (irq >= 8)
		irq -= 8;
	uint8_t value = inb(port) & ~(1 << irq);
	outb(port, value);
}

// ============================================================================
// IRQ handlers
// ============================================================================

static irq_handler_t irq_handlers[16] = {0};

void irq_install_handler(uint8_t irq, irq_handler_t handler)
{
	if (irq < 16)
		irq_handlers[irq] = handler;
}

void irq_uninstall_handler(uint8_t irq)
{
	if (irq < 16)
		irq_handlers[irq] = 0;
}

// ============================================================================
// Exception messages
// ============================================================================

static const char *exception_messages[] = {
    "Division By Zero",
    "Debug",
    "Non Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 Floating-Point Exception",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "Control Protection Exception",
};

// ============================================================================
// Handlers
// ============================================================================

void isr_handler(registers_t *regs)
{
	printk("Exception: %s (%lu)\n",
	       regs->int_no < 22 ? exception_messages[regs->int_no] : "Unknown",
	       regs->int_no);
	printk("Error code: 0x%lx\n", regs->err_code);
	printk("RIP: 0x%lx, RSP: 0x%lx\n", regs->rip, regs->rsp);

	if (regs->int_no == 8 || regs->int_no == 13 || regs->int_no == 14)
	{
		while (1)
		{
			asm volatile("cli; hlt");
		}
	}
}

void irq_handler(registers_t *regs)
{
	uint8_t irq = regs->int_no - 32;

	if (irq < 16 && irq_handlers[irq])
		irq_handlers[irq](regs);

	pic_send_eoi(irq);
}

// ============================================================================
// Syscall table
// ============================================================================

long sys_test(long a1, long a2, long a3, long a4, long a5, long a6)
{
	(void)a1;
	(void)a2;
	(void)a3;
	(void)a4;
	(void)a5;
	(void)a6;
	return 666;
}

syscall_fn_t syscall_table[SYSCALL_COUNT] = {
    [0] = (syscall_fn_t)sys_test,
    [SYS_write] = (syscall_fn_t)sys_write,
    [SYS_read] = (syscall_fn_t)sys_read,
};

uint64_t syscall_handler(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3,
			 uint64_t a4, uint64_t a5, uint64_t a6)
{
	if (num >= SYSCALL_COUNT || !syscall_table[num])
		return -1;

	return syscall_table[num](a1, a2, a3, a4, a5, a6);
}

uint64_t syscall_handler_wrapper(registers_t *regs)
{
	regs->rax = syscall_handler(regs->rax, regs->rdi, regs->rsi, regs->rdx,
				    regs->r10, regs->r8, regs->r9);
	return regs->rax;
}

// ============================================================================
// Init
// ============================================================================

#include <sys/keyboard.h>

#define KBD_BUFFER_SIZE 128

static key_event_t kbd_buffer[KBD_BUFFER_SIZE];
static volatile size_t kbd_head = 0;
static volatile size_t kbd_tail = 0;

static void kbd_push(key_event_t e)
{
	size_t next = (kbd_head + 1) % KBD_BUFFER_SIZE;
	if (next != kbd_tail)
	{
		kbd_buffer[kbd_head] = e;
		kbd_head = next;
	}
}

static bool kbd_pop(key_event_t *out)
{
	if (kbd_tail == kbd_head)
		return false;
	*out = kbd_buffer[kbd_tail];
	kbd_tail = (kbd_tail + 1) % KBD_BUFFER_SIZE;
	return true;
}

void keyboard_irq(registers_t *r)
{
	key_event_t ev = read_key_event(); // читает scancode и собирает event
	kbd_push(ev);
}

char keyboard_get_char()
{
	key_event_t ev;

	while (!kbd_pop(&ev))
	{
	} // блокируем пока нет буфера

	if (ev.released)
		return 0; // пропускаем отпускание клавиш

	return keymap_lookup_char(ev.id.scancode, ev.id.extended, ev.is_shift, ev.is_caps_lock);
}

void interrupts_init(void)
{
	pic_remap();

	irq_clear_mask(1); // включаем IRQ1 (клавиатуру)
	irq_install_handler(1, keyboard_irq);

	asm volatile("sti");

	char c = keyboard_get_char();
	printk("Got char: %c\n", c);
}
