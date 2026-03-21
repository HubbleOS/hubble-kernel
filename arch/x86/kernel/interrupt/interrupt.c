#include "interrupt/interrupt.h"
#include <syscalls/syscall_entry.h>
#include <sys/syscall.h>
#include <stdint.h>
#include "printk.h"
#include <io.h>
#include <apic/apic.h>
#include <smp/scheduler.h>
#include "sys/syscall.h"

// ============================================================================
// Legacy PIC functions (kept for fallback/compatibility)
// ============================================================================

#define PIC1_COMMAND 0x20
#define PIC1_DATA 0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA 0xA1
#define PIC_EOI 0x20

static bool using_apic = false;

void pic_disable(void)
{
	// Mask all interrupts on both PICs
	outb(PIC1_DATA, 0xFF);
	outb(PIC2_DATA, 0xFF);
	printk("Legacy PIC disabled\n");
}

void pic_remap(void)
{
	outb(PIC1_COMMAND, 0x11);
	outb(PIC2_COMMAND, 0x11);
	// ICW2: remap to vectors 0x20 (32) and 0x28 (40)
	outb(PIC1_DATA, 32);
	outb(PIC2_DATA, 40);
	// ICW3: cascade
	outb(PIC1_DATA, 4);
	outb(PIC2_DATA, 2);
	// ICW4: 8086 mode
	outb(PIC1_DATA, 0x01);
	outb(PIC2_DATA, 0x01);

	// DON'T restore a1/a2 here — mask everything immediately
	outb(PIC1_DATA, 0xFF);
	outb(PIC2_DATA, 0xFF);
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
// IRQ handlers (supports both PIC and APIC)
// ============================================================================

static irq_handler_t irq_handlers[256] = {0}; // Extended for APIC vectors

void irq_install_handler(uint8_t irq, irq_handler_t handler)
{
	if (irq < 256)
		irq_handlers[irq] = handler;
}

void irq_uninstall_handler(uint8_t irq)
{
	if (irq < 256)
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

#include "higher_half.h"

void isr_handler(registers_t *regs)
{
	printk("\n\tEXCEPTION OCCURRED\n");

	printk("Exception: %s (%lu)\n",
	       regs->int_no < 22 ? exception_messages[regs->int_no] : "Unknown",
	       regs->int_no);
	printk("Error code: 0x%lx\n", regs->err_code);

	printk("\n=== Registers ===\n");
	printk("RIP: 0x%016lx    RSP: 0x%016lx\n", regs->rip, regs->rsp);
	printk("RAX: 0x%016lx    RBX: 0x%016lx\n", regs->rax, regs->rbx);
	printk("RCX: 0x%016lx    RDX: 0x%016lx\n", regs->rcx, regs->rdx);
	printk("RSI: 0x%016lx    RDI: 0x%016lx\n", regs->rsi, regs->rdi);
	printk("RBP: 0x%016lx    R8:  0x%016lx\n", regs->rbp, regs->r8);
	printk("R9:  0x%016lx    R10: 0x%016lx\n", regs->r9, regs->r10);
	printk("R11: 0x%016lx    R12: 0x%016lx\n", regs->r11, regs->r12);
	printk("R13: 0x%016lx    R14: 0x%016lx\n", regs->r13, regs->r14);
	printk("R15: 0x%016lx\n", regs->r15);

	printk("\n=== Segments ===\n");
	printk("SS:  0x%04lx\n", regs->ss);
	printk("RFLAGS: 0x%016lx\n", regs->rflags);

	if (regs->int_no == 8 || regs->int_no == 13 || regs->int_no == 14)
	{
		printk("\nFATAL ERROR - System Halted\n");
		while (1)
		{
			asm volatile("cli; hlt");
		}
	}
	else
	{
		if (is_scheduler_initialized())
		{
			task_exit(-1);
		}
	}
}

void irq_handler(registers_t *regs)
{
	uint8_t irq = regs->int_no - 32;
	// outb(0x3f8, irq + '0');
	// Check for PIC spurious IRQ before doing anything
	if (irq == 7)
	{
		// Check master PIC ISR
		outb(PIC1_COMMAND, 0x0B); // Read ISR
		if (!(inb(PIC1_COMMAND) & 0x80))
			return; // Spurious — no EOI
	}
	if (irq == 15)
	{
		// Check slave PIC ISR
		outb(PIC2_COMMAND, 0x0B);
		if (!(inb(PIC2_COMMAND) & 0x80))
		{
			outb(PIC1_COMMAND, PIC_EOI); // Still need master EOI
			return;
		}
	}

	if (irq_handlers[irq])
		irq_handlers[irq](regs);

	if (using_apic && apic_is_initialized())
		lapic_eoi();
	else
		pic_send_eoi(irq);
}

// ============================================================================
// Syscall table
// ============================================================================

typedef long (*syscall_fn_t)(long arg1, long arg2, long arg3, long arg4, long arg5, long arg6);

#define SYSCALL_COUNT 256

syscall_fn_t syscall_table[SYSCALL_COUNT] = {
    [SYS_write] = (syscall_fn_t)sys_write,
    [0] = (syscall_fn_t)sys_read,
    [3] = (syscall_fn_t)sys_mmap,
    [4] = (syscall_fn_t)sys_open,
    [5] = (syscall_fn_t)sys_close,
    [6] = (syscall_fn_t)sys_spawn};

uint64_t syscall_handler(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3,
			 uint64_t a4, uint64_t a5, uint64_t a6)
{
	if (num >= SYSCALL_COUNT || !syscall_table[num])
		return -1;

	return syscall_table[num](a1, a2, a3, a4, a5, a6);
}

uint64_t syscall_handler_wrapper(registers_t *regs)
{
	regs->rax = syscall_handler(
	    regs->rax,
	    regs->rdi,
	    regs->rsi,
	    regs->rdx,
	    regs->r10,
	    regs->r8,
	    regs->r9);

	return regs->rax;
}

// ============================================================================
// Init
// ============================================================================

#include <dev/keyboard.h>
#include <acpi/acpi.h>

void interrupts_init(void)
{
	printk("Initializing interrupt system...\n");

	// Try to initialize APIC
	if (acpi_is_initialized() && apic_init() == 0)
	{
		printk("Using APIC for interrupt handling\n");
		using_apic = true;
		// 1. Remap PIC away from CPU exception vectors FIRST
		pic_remap();

		// 2. NOW disable (mask all) — safe, no vector collisions
		pic_disable();
		// Enable Local APIC on BSP
		lapic_enable();

		// Register keyboard handler on IRQ 1 (vector 33)
		// irq_install_handler(1, keyboard_irq);

		// Unmask keyboard interrupt in I/O APIC
		ioapic_unmask_irq(1);
		ioapic_unmask_irq(2);
		ioapic_unmask_irq(12);
		irq_install_handler(1, keyboard_irq);

		// Optional: Setup LAPIC timer for preemptive multitasking
		lapic_timer_init(100);			     // 100 Hz timer
		irq_install_handler(0, lapic_timer_handler); // Timer on vector 32
	}
	else
	{
		printk("APIC not available, falling back to PIC\n");
		using_apic = false;

		// Use legacy PIC
		pic_remap();
		irq_clear_mask(1); // Enable keyboard IRQ
		irq_install_handler(1, keyboard_irq);
	}

	// Enable interrupts
	asm volatile("sti");
	printk("Interrupts enabled\n");
}
