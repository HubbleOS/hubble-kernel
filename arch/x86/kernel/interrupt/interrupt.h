/**
 * @file interrupt.h
 * @brief Interrupt handling functions
 */

#pragma once

#include <stdint.h>

/**
 * @brief CPU register state snapshot pushed during an interrupt/exception.
 *
 * This structure contains:
 * - General purpose registers saved manually in ISR stubs
 * - Interrupt number and optional error code
 * - CPU-pushed state (RIP, CS, RFLAGS, RSP, SS)
 */
typedef struct registers
{
	uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
	uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;

	uint64_t int_no, err_code;

	uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed)) registers_t;

/**
 * @brief Type definition for IRQ handler functions.
 *
 * @param regs Pointer to saved CPU register state
 */
typedef void (*irq_handler_t)(registers_t *regs);

/**
 * @brief Initialize interrupt subsystem (PIC/APIC, handlers, enable interrupts).
 */
void interrupts_init(void);

/**
 * @brief Remap legacy PIC interrupt vectors to avoid CPU exception overlap.
 */
void pic_remap(void);

/**
 * @brief Send End Of Interrupt (EOI) signal to PIC.
 *
 * @param irq IRQ number that has been handled
 */
void pic_send_eoi(uint8_t irq);

/**
 * @brief Mask (disable) a specific IRQ line on PIC.
 *
 * @param irq IRQ number to disable
 */
void irq_set_mask(uint8_t irq);

/**
 * @brief Unmask (enable) a specific IRQ line on PIC.
 *
 * @param irq IRQ number to enable
 */
void irq_clear_mask(uint8_t irq);

/**
 * @brief Register a custom handler for a specific IRQ.
 *
 * @param irq IRQ number
 * @param handler Function to handle the interrupt
 */
void irq_install_handler(uint8_t irq, irq_handler_t handler);

/**
 * @brief Remove handler for a specific IRQ.
 *
 * @param irq IRQ number
 */
void irq_uninstall_handler(uint8_t irq);

/**
 * @brief Common handler for CPU exceptions (ISRs).
 *
 * Called from assembly stubs when a CPU exception occurs.
 *
 * @param regs Pointer to saved register state
 */
void isr_handler(registers_t *regs);

/**
 * @brief Common handler for hardware interrupts (IRQs).
 *
 * Dispatches to registered handlers and sends EOI.
 *
 * @param regs Pointer to saved register state
 */
void irq_handler(registers_t *regs);
