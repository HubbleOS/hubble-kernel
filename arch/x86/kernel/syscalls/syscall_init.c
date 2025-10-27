#include <stdint.h>

#define MSR_EFER 0xC0000080
#define MSR_STAR 0xC0000081
#define MSR_LSTAR 0xC0000082
#define MSR_SFMASK 0xC0000084

#define EFER_SCE (1 << 0) // System Call Extensions

// Внешняя функция из ASM
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
	// 1. Включаем SYSCALL extension
	uint64_t efer = rdmsr(MSR_EFER);
	efer |= EFER_SCE;
	wrmsr(MSR_EFER, efer);

	// 2. STAR: сегменты
	uint64_t star = 0;
	star |= ((uint64_t)0x08 << 32); // Kernel CS
	star |= ((uint64_t)0x18 << 48); // User CS base
	wrmsr(MSR_STAR, star);

	// 3. LSTAR: адрес handler
	wrmsr(MSR_LSTAR, (uint64_t)syscall_entry);

	// 4. SFMASK: сбрасываем IF и DF
	wrmsr(MSR_SFMASK, 0x200 | 0x400);
}
