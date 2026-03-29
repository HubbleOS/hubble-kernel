#include "interrupt.h"
#include <syscalls/syscall_entry.h>
#include <stdint.h>
#include <hubble/printk.h>
#include <io.h>
#include <apic/apic.h>
#include <smp/scheduler.h>

#include <asm.h>

// Legacy PIC functions (kept for fallback/compatibility)
#define PIC1_COMMAND 0x20
#define PIC1_DATA 0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA 0xA1
#define PIC_EOI 0x20

static bool using_apic = false;

/**
 * @brief Disable legacy PIC by masking all IRQ lines.
 *
 * Used when switching to APIC mode.
 */
void pic_disable(void)
{
	// Mask all interrupts on both PICs
	outb(PIC1_DATA, 0xFF);
	outb(PIC2_DATA, 0xFF);
	printk("Legacy PIC disabled\n");
}

/**
 * @brief Remap PIC interrupt vectors.
 *
 * Moves IRQs from default (0–15) to 32–47 to avoid collision
 * with CPU exceptions (0–31).
 */
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

/**
 * @brief Send End Of Interrupt (EOI) to PIC controllers.
 *
 * Required after handling an IRQ to allow further interrupts.
 *
 * @param irq IRQ number
 */
void pic_send_eoi(uint8_t irq)
{
	if (irq >= 8)
		outb(PIC2_COMMAND, PIC_EOI);
	outb(PIC1_COMMAND, PIC_EOI);
}

/**
 * @brief Mask (disable) a specific IRQ line on PIC.
 *
 * @param irq IRQ number
 */
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

// IRQ handlers (supports both PIC and APIC)
static irq_handler_t irq_handlers[256] = {0}; // Extended for APIC vectors

/**
 * @brief Install an interrupt handler for a given IRQ vector.
 *
 * Supports up to 256 vectors (for APIC compatibility).
 *
 * @param irq IRQ/vector number
 * @param handler Handler function
 */
void irq_install_handler(uint8_t irq, irq_handler_t handler)
{
	if (irq < 256)
		irq_handlers[irq] = handler;
}

/**
 * @brief Uninstall interrupt handler for a given IRQ vector.
 *
 * @param irq IRQ/vector number
 */
void irq_uninstall_handler(uint8_t irq)
{
	if (irq < 256)
		irq_handlers[irq] = 0;
}

// Exception messages
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

// Handlers
/**
 * @brief Handle CPU exceptions (faults, traps, aborts).
 *
 * Prints diagnostic information and halts on critical faults.
 * Non-fatal exceptions terminate the current task if scheduler is active.
 *
 * @param regs Pointer to register snapshot
 */
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

/**
 * @brief Handle hardware interrupts (IRQs).
 *
 * - Detects and ignores spurious IRQs (PIC-specific)
 * - Dispatches to registered handler
 * - Sends EOI via PIC or APIC
 *
 * @param regs Pointer to register snapshot
 */
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

// Init
#include <dev/keyboard.h>
#include <acpi/acpi.h>

/**
 * @brief Initialize interrupt handling subsystem.
 *
 * Steps:
 * - Detect and initialize APIC (if available)
 * - Fallback to legacy PIC otherwise
 * - Setup IRQ routing (keyboard, timer, etc.)
 * - Enable CPU interrupts (STI)
 */
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
	sti();
	printk("Interrupts enabled\n");
}
