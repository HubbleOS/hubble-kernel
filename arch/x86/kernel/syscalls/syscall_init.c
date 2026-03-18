#include "syscall_entry.h"
#include "printk.h"
#include <apic/apic.h>
#include <stdint.h>

#define MSR_EFER 0xC0000080
#define MSR_STAR 0xC0000081
#define MSR_LSTAR 0xC0000082
#define MSR_SFMASK 0xC0000084
#define EFER_SCE (1 << 0)

extern void syscall_entry(void);

// Отдельный стек для syscall (16 KB)
#define MAX_CPUS 8
static uint8_t syscall_stacks[MAX_CPUS][16384] __attribute__((aligned(16)));
uint64_t syscall_rsp0 = 0;

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

	// Инициализируем указатель на вершину стека
	uint8_t cpu_id = lapic_get_id();

	// per-CPU stack
	syscall_rsp0 = (uint64_t)(syscall_stacks[cpu_id] + sizeof(syscall_stacks[cpu_id]));

	printk("  Syscall stack at 0x%016llx\n", syscall_rsp0);

	// 1. Включаем SYSCALL Extension
	uint64_t efer = rdmsr(MSR_EFER);
	efer |= EFER_SCE;
	wrmsr(MSR_EFER, efer);
	printk("  EFER.SCE enabled\n");

	// 2. Настраиваем STAR - ИЗМЕНЕНО!
	uint64_t star = 0;
	star |= ((uint64_t)0x08 << 32); // SYSCALL CS = 0x08 (Kernel Code)
	star |= ((uint64_t)0x10 << 48); // SYSRET base = 0x10 (Kernel Data)
	wrmsr(MSR_STAR, star);
	printk("  STAR = 0x%016llx\n", star);

	// CS = (0x10 + 16) | 3 = 0x20 | 3 = 0x23 (User Code)
	// SS = (0x10 + 8) | 3 = 0x18 | 3 = 0x1B (User Data)

	// 3. Устанавливаем обработчик
	wrmsr(MSR_LSTAR, (uint64_t)syscall_entry);
	printk("  LSTAR = 0x%016llx\n", (uint64_t)syscall_entry);

	// 4. SFMASK
	wrmsr(MSR_SFMASK, 0x100 | 0x200 | 0x400); // IF | DF | TF
	printk("  SFMASK = 0x%llx\n", 0x700ULL);

	printk("SYSCALL/SYSRET initialized successfully\n");
}
