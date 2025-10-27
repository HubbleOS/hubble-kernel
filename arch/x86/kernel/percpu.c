// kernel/arch/x86_64/percpu.c
#include "percpu.h"

#include <stddef.h>

#define MSR_GS_BASE 0xC0000101
#define MSR_KERNEL_GS_BASE 0xC0000102

// Массив данных для каждого CPU
percpu_data_t percpu_data[256] __attribute__((aligned(64)));

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

// Получить APIC ID (CPU ID)
uint32_t get_cpu_id(void)
{
	uint32_t eax, ebx, ecx, edx;
	__asm__ volatile("cpuid"
			 : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
			 : "a"(1));
	return (ebx >> 24) & 0xFF; // APIC ID в битах 24-31
}

void percpu_init(void)
{
	uint32_t cpu_id = get_cpu_id();

	// Инициализируем данные для текущего CPU
	percpu_data[cpu_id].cpu_id = cpu_id;
	percpu_data[cpu_id].kernel_stack = 0; // Установишь позже
	percpu_data[cpu_id].user_stack = 0;
	percpu_data[cpu_id].current_task = NULL;

	// Устанавливаем MSR_KERNEL_GS_BASE на адрес наших данных
	// После swapgs этот адрес попадёт в GS
	wrmsr(MSR_KERNEL_GS_BASE, (uint64_t)&percpu_data[cpu_id]);

	// MSR_GS_BASE будет использоваться в userspace (если нужно)
	// wrmsr(MSR_GS_BASE, 0);  // или адрес TLS для userspace
}

// Установить kernel stack для текущего CPU
void percpu_set_kernel_stack(uint64_t stack)
{
	uint32_t cpu_id = get_cpu_id();
	percpu_data[cpu_id].kernel_stack = stack;
}
