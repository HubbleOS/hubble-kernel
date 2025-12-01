#include <stdint.h>
#include "printk.h"

#define MSR_EFER 0xC0000080
#define MSR_STAR 0xC0000081
#define MSR_LSTAR 0xC0000082
#define MSR_SFMASK 0xC0000084

#define EFER_SCE (1 << 0)

extern void syscall_entry(void);

static inline void wrmsr(uint32_t msr, uint64_t value)
{
	uint32_t low = value & 0xFFFFFFFF;
	uint32_t high = value >> 32;
	__asm__ volatile("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

static inline uint64_t rdmsr(uint32_t msr)
{
	uint32_t low, high;
	__asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
	return ((uint64_t)high << 32) | low;
}

void syscall_init(void)
{
	printk("Initializing SYSCALL/SYSRET...\n");

	// 1. Включаем SCE (System Call Extensions)
	uint64_t efer = rdmsr(MSR_EFER);
	efer |= EFER_SCE;
	wrmsr(MSR_EFER, efer);

	// 2. STAR: задаём селекторы сегментов
	// Биты 32-47: Kernel CS selector для SYSCALL (0x08)
	// Биты 48-63: User CS base для SYSRET
	//   При SYSRET: CS = (STAR[63:48] + 16) | 3
	//               SS = (STAR[63:48] + 8) | 3
	//   Если User CS = 0x18 (24) и User SS = 0x20 (32):
	//   То STAR[63:48] должен быть 0x08 (потому что 0x08 + 16 = 0x18)

	uint64_t star = ((uint64_t)0x08 << 48) | ((uint64_t)0x08 << 32);
	wrmsr(MSR_STAR, star);
	printk("  STAR = 0x%016lx\n", star);

	// 3. LSTAR: адрес обработчика syscall
	wrmsr(MSR_LSTAR, (uint64_t)syscall_entry);
	printk("  LSTAR = 0x%016lx\n", (uint64_t)syscall_entry);

	// 4. SFMASK: маска флагов, которые нужно сбросить при SYSCALL
	// 0x200 = IF (Interrupt Flag) - отключаем прерывания
	// 0x002 = Reserved (всегда 1 в RFLAGS)
	// 0x100 = TF (Trap Flag)
	wrmsr(MSR_SFMASK, 0x200);

	printk("SYSCALL/SYSRET initialized\n");
}
