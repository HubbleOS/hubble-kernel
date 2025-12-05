#include "gdt/interrupt.h"
#include <syscalls/syscall.h>
#include <sys/syscall.h>
#include <stdint.h>
#include "printk.h"
#include <io.h>

// ============================================================================
// PIC функции
// ============================================================================

#define PIC1_COMMAND 0x20
#define PIC1_DATA 0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA 0xA1
#define PIC_EOI 0x20

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

#include "higher_half.h"

void isr_handler(registers_t *regs)
{
	if (regs->int_no == 14)
	{
		// Page Fault - special handling
		uint64_t cr2;
		asm volatile("mov %%cr2, %0" : "=r"(cr2));

		printk("\n\tPAGE FAULT\n");
		printk("Faulting address: 0x%016llx\n", (unsigned long long)cr2);
		printk("Error code: 0x%lx\n", regs->err_code);

		// Decode error code
		printk("\nError Code Details:\n");
		printk("  [P] Page %s\n", (regs->err_code & 1) ? "present" : "not present");
		printk("  [W/R] %s\n", (regs->err_code & 2) ? "write" : "read");
		printk("  [U/S] %s mode\n", (regs->err_code & 4) ? "user" : "supervisor");
		printk("  [RSVD] %s\n", (regs->err_code & 8) ? "reserved bit violation" : "OK");
		printk("  [I/D] %s\n", (regs->err_code & 16) ? "instruction fetch" : "data access");

		printk("\n=== Registers ===\n");
		printk("RIP: 0x%016lx    RSP: 0x%016lx\n", regs->rip, regs->rsp);
		printk("RAX: 0x%016lx    RBX: 0x%016lx\n", regs->rax, regs->rbx);
		printk("RCX: 0x%016lx    RDX: 0x%016lx\n", regs->rcx, regs->rdx);
		printk("RSI: 0x%016lx    RDI: 0x%016lx\n", regs->rsi, regs->rdi);
		printk("RBP: 0x%016lx\n", regs->rbp);

		printk("\n=== Segments ===\n");
		printk("CS: 0x%04lx    SS: 0x%04lx\n", regs->cs, regs->ss);
		printk("RFLAGS: 0x%016lx\n", regs->rflags);

		// Check page table entries
		printk("\n=== Page Table Walk ===\n");

		uint64_t cr3;
		asm volatile("mov %%cr3, %0" : "=r"(cr3));
		printk("CR3 (PML4 physical): 0x%llx\n", (unsigned long long)cr3);

		// Use higher-half mapping to access page tables
		uint64_t *pml4 = (uint64_t *)PHYS_TO_VIRT(cr3 & ~0xFFFULL);
		uint64_t pml4_idx = (cr2 >> 39) & 0x1FF;
		printk("PML4[%llu] = 0x%016llx ",
		       (unsigned long long)pml4_idx,
		       (unsigned long long)pml4[pml4_idx]);
		if (pml4[pml4_idx] & 1)
		{
			printk("[P=%d W=%d U=%d]\n",
			       !!(pml4[pml4_idx] & 1),
			       !!(pml4[pml4_idx] & 2),
			       !!(pml4[pml4_idx] & 4));

			uint64_t *pdpt = (uint64_t *)PHYS_TO_VIRT(pml4[pml4_idx] & ~0xFFFULL);
			uint64_t pdpt_idx = (cr2 >> 30) & 0x1FF;
			printk("PDPT[%llu] = 0x%016llx ",
			       (unsigned long long)pdpt_idx,
			       (unsigned long long)pdpt[pdpt_idx]);
			if (pdpt[pdpt_idx] & 1)
			{
				printk("[P=%d W=%d U=%d]\n",
				       !!(pdpt[pdpt_idx] & 1),
				       !!(pdpt[pdpt_idx] & 2),
				       !!(pdpt[pdpt_idx] & 4));

				uint64_t *pd = (uint64_t *)PHYS_TO_VIRT(pdpt[pdpt_idx] & ~0xFFFULL);
				uint64_t pd_idx = (cr2 >> 21) & 0x1FF;
				printk("PD[%llu] = 0x%016llx ",
				       (unsigned long long)pd_idx,
				       (unsigned long long)pd[pd_idx]);
				if (pd[pd_idx] & 1)
				{
					printk("[P=%d W=%d U=%d HUGE=%d]\n",
					       !!(pd[pd_idx] & 1),
					       !!(pd[pd_idx] & 2),
					       !!(pd[pd_idx] & 4),
					       !!(pd[pd_idx] & 0x80));

					if (!(pd[pd_idx] & 0x80))
					{ // Not huge page
						uint64_t *pt = (uint64_t *)PHYS_TO_VIRT(pd[pd_idx] & ~0xFFFULL);
						uint64_t pt_idx = (cr2 >> 12) & 0x1FF;
						printk("PT[%llu] = 0x%016llx ",
						       (unsigned long long)pt_idx,
						       (unsigned long long)pt[pt_idx]);
						if (pt[pt_idx] & 1)
						{
							printk("[P=%d W=%d U=%d NX=%d]\n",
							       !!(pt[pt_idx] & 1),
							       !!(pt[pt_idx] & 2),
							       !!(pt[pt_idx] & 4),
							       !!(pt[pt_idx] & (1ULL << 63)));
							printk("Physical address: 0x%llx\n",
							       (unsigned long long)(pt[pt_idx] & ~0xFFFULL));
						}
						else
						{
							printk("[NOT PRESENT]\n");
						}
					}
				}
				else
				{
					printk("[NOT PRESENT]\n");
				}
			}
			else
			{
				printk("[NOT PRESENT]\n");
			}
		}
		else
		{
			printk("[NOT PRESENT]\n");
		}

		printk("\nFATAL ERROR - System Halted\n");
		while (1)
		{
			asm volatile("cli; hlt");
		}
	}

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

	// Спроба декодувати помилку GPF
	if (regs->int_no == 13 && regs->err_code != 0)
	{
		printk("\n=== GPF Error Code Details ===\n");
		if (regs->err_code & 1)
			printk("External event (hardware interrupt)\n");
		else
			printk("Internal event (software exception)\n");

		uint8_t tbl = (regs->err_code >> 1) & 0x3;
		printk("Table: ");
		switch (tbl)
		{
		case 0:
			printk("GDT\n");
			break;
		case 1:
			printk("IDT\n");
			break;
		case 2:
			printk("LDT\n");
			break;
		case 3:
			printk("IDT\n");
			break;
		}

		uint16_t index = (regs->err_code >> 3);
		printk("Selector Index: %u (0x%x)\n", index, index);
	}

	if (regs->int_no == 8 || regs->int_no == 13 || regs->int_no == 14)
	{
		printk("\nFATAL ERROR - System Halted\n");
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

long sys_test()
{
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
	printk("%d\n", num);
	// num = 1;

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

#include <sys/keyboard.h>

void interrupts_init(void)
{
	pic_remap();

	irq_clear_mask(1); // включаем IRQ1 (клавиатуру)
	irq_install_handler(1, keyboard_irq);

	asm volatile("sti");
}
