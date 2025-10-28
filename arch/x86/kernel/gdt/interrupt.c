#include "interrupt.h"
#include <stdint.h>

// ============================================================================
// PIC (Programmable Interrupt Controller) функции
// ============================================================================

#define PIC1_COMMAND 0x20
#define PIC1_DATA 0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA 0xA1
#define PIC_EOI 0x20

// Inline assembly для портов ввода/вывода
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

// Перепрограммирование PIC
void pic_remap(void)
{
	uint8_t a1, a2;

	// Сохраняем маски
	a1 = inb(PIC1_DATA);
	a2 = inb(PIC2_DATA);

	// Начинаем инициализацию
	outb(PIC1_COMMAND, 0x11);
	outb(PIC2_COMMAND, 0x11);

	// Устанавливаем векторы: Master PIC -> IRQ 32-39, Slave PIC -> IRQ 40-47
	outb(PIC1_DATA, 32);
	outb(PIC2_DATA, 40);

	// Настраиваем каскадирование
	outb(PIC1_DATA, 4); // IRQ2 подключен к slave
	outb(PIC2_DATA, 2); // Slave в каскаде

	// Устанавливаем режим 8086
	outb(PIC1_DATA, 0x01);
	outb(PIC2_DATA, 0x01);

	// Восстанавливаем маски
	outb(PIC1_DATA, a1);
	outb(PIC2_DATA, a2);
}

// Отправка End of Interrupt
void pic_send_eoi(uint8_t irq)
{
	if (irq >= 8)
		outb(PIC2_COMMAND, PIC_EOI);

	outb(PIC1_COMMAND, PIC_EOI);
}

// Маскирование IRQ
void irq_set_mask(uint8_t irq)
{
	uint16_t port;
	uint8_t value;

	if (irq < 8)
	{
		port = PIC1_DATA;
	}
	else
	{
		port = PIC2_DATA;
		irq -= 8;
	}

	value = inb(port) | (1 << irq);
	outb(port, value);
}

// Размаскирование IRQ
void irq_clear_mask(uint8_t irq)
{
	uint16_t port;
	uint8_t value;

	if (irq < 8)
	{
		port = PIC1_DATA;
	}
	else
	{
		port = PIC2_DATA;
		irq -= 8;
	}

	value = inb(port) & ~(1 << irq);
	outb(port, value);
}

// ============================================================================
// Таблица обработчиков IRQ
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
// Exception names
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
// Основные обработчики
// ============================================================================

// Обработчик CPU исключений
void isr_handler(registers_t *regs)
{
	// Если есть printk, выводим информацию
	// printk("Exception: %s (%d)\n",
	//        regs->int_no < 22 ? exception_messages[regs->int_no] : "Unknown",
	//        regs->int_no);
	// printk("Error code: 0x%lx\n", regs->err_code);
	// printk("RIP: 0x%lx, RSP: 0x%lx\n", regs->rip, regs->rsp);
	// printk("RAX: 0x%lx, RBX: 0x%lx\n", regs->rax, regs->rbx);

	// Критические ошибки - останавливаем систему
	if (regs->int_no == 8 || regs->int_no == 13 || regs->int_no == 14)
	{
		// Double Fault, GPF, или Page Fault
		while (1)
		{
			asm volatile("cli; hlt");
		}
	}
}

// Обработчик аппаратных прерываний
void irq_handler(registers_t *regs)
{
	uint8_t irq = regs->int_no - 32;

	// Вызываем зарегистрированный обработчик, если есть
	if (irq < 16 && irq_handlers[irq])
	{
		irq_handlers[irq](regs);
	}

	// Отправляем EOI
	pic_send_eoi(irq);
}

#include <sys/syscall.h>
#include <syscalls/syscall.h>

long sys_test(void)
{
	return 666;
}

syscall_fn_t syscall_table[SYSCALL_COUNT] = {
    [SYS_write] = (syscall_fn_t)sys_write,
    [SYS_read] = (syscall_fn_t)sys_read,
    [0] = (syscall_fn_t)sys_test,
};

// Обработчик системных вызовов
uint64_t syscall_handler(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3,
			 uint64_t a4, uint64_t a5, uint64_t a6)
{
	if (num >= SYSCALL_COUNT)
		return -1;

	syscall_fn_t fn = syscall_table[num];
	if (!fn)
		return -1;

	return fn(a1, a2, a3, a4, a5, a6);
}

uint64_t syscall_handler_wrapper(registers_t *regs)
{
	regs->rax = syscall_handler(regs->rax, regs->rdi, regs->rsi, regs->rdx,
				    regs->r10, regs->r8, regs->r9);
	return regs->rax;
}

// ============================================================================
// Инициализация системы прерываний
// ============================================================================

void interrupts_init(void)
{
	// Перепрограммируем PIC
	pic_remap();

	// Разрешаем прерывания
	asm volatile("sti");
}
