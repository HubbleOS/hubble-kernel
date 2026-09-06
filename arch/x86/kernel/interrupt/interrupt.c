/**
 * @file interrupt.c
 * @brief Interrupt handling: PIC, IRQ dispatch, exception handlers
 *
 * Manages the legacy 8259 PIC (used before or alongside APIC),
 * provides an IRQ handler registration table, and implements
 * the common CPU exception and hardware interrupt dispatchers.
 */

#include "interrupt.h"
#include <asm.h>
#include <hubble/printk.h>
#include <io.h>

#include <apic/apic.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <smp/scheduler.h>

/* -- Legacy PIC Constants -------------------------------------- */

#define PIC1_COMMAND 0x20
#define PIC1_DATA 0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA 0xA1
#define PIC_EOI 0x20

/* -- PIC Functions --------------------------------------------- */

/**
 * @brief Disable the legacy PIC by masking all IRQs
 */
void pic_disable(void) {
  outb(PIC1_DATA, 0xFF);
  outb(PIC2_DATA, 0xFF);
  printk(KERN_INFO "Legacy PIC disabled\n");
}

/**
 * @brief Remap PIC vectors to avoid overlap with CPU exceptions
 *
 * Moves IRQs from default 0-15 to vectors 32-47.
 */
void pic_remap(void) {
  outb(PIC1_COMMAND, 0x11);
  outb(PIC2_COMMAND, 0x11);

  outb(PIC1_DATA, 32);
  outb(PIC2_DATA, 40);

  outb(PIC1_DATA, 4);
  outb(PIC2_DATA, 2);

  outb(PIC1_DATA, 0x01);
  outb(PIC2_DATA, 0x01);

  outb(PIC1_DATA, 0xFF);
  outb(PIC2_DATA, 0xFF);
}

/**
 * @brief Send End-Of-Interrupt to the PIC
 *
 * @param irq IRQ number that was handled
 */
void pic_send_eoi(uint8_t irq) {
  if (irq >= 8)
    outb(PIC2_COMMAND, PIC_EOI);
  outb(PIC1_COMMAND, PIC_EOI);
}

/**
 * @brief Mask (disable) a specific IRQ line on the PIC
 *
 * @param irq IRQ number to disable
 */
void irq_set_mask(uint8_t irq) {
  uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
  if (irq >= 8)
    irq -= 8;
  uint8_t value = inb(port) | (1 << irq);
  outb(port, value);
}

/**
 * @brief Unmask (enable) a specific IRQ line on the PIC
 *
 * @param irq IRQ number to enable
 */
void irq_clear_mask(uint8_t irq) {
  uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
  if (irq >= 8)
    irq -= 8;
  uint8_t value = inb(port) & ~(1 << irq);
  outb(port, value);
}

/* -- IRQ Handler Table ----------------------------------------- */

static irq_handler_t irq_handlers[256] = {0};

/**
 * @brief Register an interrupt handler for a given vector
 *
 * @param irq     IRQ / vector number
 * @param handler Handler function
 */
void irq_install_handler(uint8_t irq, irq_handler_t handler) {
  if (irq < 256)
    irq_handlers[irq] = handler;
}

/**
 * @brief Unregister an interrupt handler
 *
 * @param irq IRQ / vector number
 */
void irq_uninstall_handler(uint8_t irq) {
  if (irq < 256)
    irq_handlers[irq] = 0;
}

/* -- Exception Messages ---------------------------------------- */

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

/* -- Common Exception Handler ---------------------------------- */

/**
 * @brief Handle CPU exceptions (faults, traps, aborts)
 *
 * Prints diagnostic information and halts on fatal faults
 * (double fault, GPF, page fault).  Non-fatal exceptions
 * terminate the current task if the scheduler is active.
 *
 * @param regs Register snapshot from the ISR stub
 */
void isr_handler(registers_t *regs) {
  /* Page fault, present + write + from user mode: may be a COW page
   * waiting to be duplicated rather than a real error - resolve it
   * silently before spending any diagnostic output on what is normal COW
   * behavior. The user-mode check matters: COW only ever applies to user
   * mappings, and without it a kernel-mode fault landing on a page whose
   * PTE happens to carry stray bits that look like PRESENT|COW gets
   * misrouted into vmm_resolve_cow(), which then does real alloc/copy/free
   * work against a bogus physical address. */
  if (regs->int_no == 14 && (regs->err_code & 0x7) == 0x7) {
    uint64_t fault_addr;
    asm volatile("mov %%cr2, %0" : "=r"(fault_addr));
    if (vmm_resolve_cow(fault_addr))
      return;
  }

  printk(KERN_INFO "\n\tEXCEPTION OCCURRED\n");

  printk(KERN_INFO "Exception: %s (%lu)\n",
         regs->int_no < 22 ? exception_messages[regs->int_no] : "Unknown",
         regs->int_no);
  printk(KERN_ERR "Error code: 0x%lx\n", regs->err_code);

  printk(KERN_INFO "Registers");
  printk(KERN_INFO "RIP: 0x%016lx    RSP: 0x%016lx\n", regs->rip, regs->rsp);
  printk(KERN_INFO "RAX: 0x%016lx    RBX: 0x%016lx\n", regs->rax, regs->rbx);
  printk(KERN_INFO "RCX: 0x%016lx    RDX: 0x%016lx\n", regs->rcx, regs->rdx);
  printk(KERN_INFO "RSI: 0x%016lx    RDI: 0x%016lx\n", regs->rsi, regs->rdi);
  printk(KERN_INFO "RBP: 0x%016lx    R8:  0x%016lx\n", regs->rbp, regs->r8);
  printk(KERN_INFO "R9:  0x%016lx    R10: 0x%016lx\n", regs->r9, regs->r10);
  printk(KERN_INFO "R11: 0x%016lx    R12: 0x%016lx\n", regs->r11, regs->r12);
  printk(KERN_INFO "R13: 0x%016lx    R14: 0x%016lx\n", regs->r13, regs->r14);
  printk(KERN_INFO "R15: 0x%016lx\n", regs->r15);

  printk(KERN_INFO "Segments");
  printk(KERN_INFO "SS:  0x%04lx\n", regs->ss);
  printk(KERN_INFO "RFLAGS: 0x%016lx\n", regs->rflags);

  uint64_t cr2;
  asm volatile("mov %%cr2, %0" : "=r"(cr2));
  printk("CR2 = %p\n", cr2);

  if (regs->int_no == 8 || regs->int_no == 13 || regs->int_no == 14) {
    printk(KERN_ERR "\nFATAL ERROR - System Halted\n");

    if (regs->int_no == 14) {
      uint64_t addr;
      asm volatile("mov %%cr2, %0" : "=r"(addr));

      debug_dump_mapping((uint64_t *)get_cr3(), addr);

      uint64_t cr3;
      asm volatile("mov %%cr3, %0" : "=r"(cr3));
      task_t *t = get_current_task();
      printk(KERN_INFO
             "Fault: active CR3=0x%llx task->page_table=0x%llx match=%d\n",
             cr3, (uint64_t)t->mm.page_table,
             cr3 == (uint64_t)t->mm.page_table);
    }
    while (1) {
      asm volatile("cli; hlt");
    }
  } else {
    if (is_scheduler_initialized()) {
      task_exit(-1);
    }
  }
}

/* -- Common IRQ Handler ---------------------------------------- */

/**
 * @brief Handle hardware interrupts (IRQs)
 *
 * - Detects and ignores spurious IRQs on the PIC
 * - Dispatches to the registered handler
 * - Sends EOI via APIC (if available) or PIC
 *
 * @param regs Register snapshot from the ISR stub
 */
void irq_handler(registers_t *regs) {
  uint8_t irq = regs->int_no - 32;

  if (irq == 7) {
    outb(PIC1_COMMAND, 0x0B);
    if (!(inb(PIC1_COMMAND) & 0x80))
      return;
  }
  if (irq == 15) {
    outb(PIC2_COMMAND, 0x0B);
    if (!(inb(PIC2_COMMAND) & 0x80)) {
      outb(PIC1_COMMAND, PIC_EOI);
      return;
    }
  }

  if (irq_handlers[irq])
    irq_handlers[irq](regs);

  if (apic_is_initialized())
    lapic_eoi();
  else
    pic_send_eoi(irq);
}

/* -- Initialisation -------------------------------------------- */

/**
 * @brief Initialise the interrupt subsystem
 *
 * Remaps the PIC to vectors 32+, masks all IRQs, and
 * enables interrupts on the BSP.
 */
void interrupts_init(void) {
  printk(KERN_INFO "Initializing interrupt system...\n");

  pic_remap();
  pic_disable();

  sti();
  printk(KERN_OK "Interrupts enabled\n");
}
