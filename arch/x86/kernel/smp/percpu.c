#include "percpu.h"
#include <apic/apic.h>
#include <printk.h>
#include <hubble/string.h>

cpu_info_t cpu_data[MAX_CPUS];
volatile uint32_t num_cpus_online = 0;

cpu_info_t *get_current_cpu(void)
{
	uint8_t apic_id = lapic_get_id();

	// Find CPU by APIC ID
	for (int i = 0; i < MAX_CPUS; i++)
	{
		if (cpu_data[i].online && cpu_data[i].apic_id == apic_id)
			return &cpu_data[i];
	}

	return NULL;
}

cpu_info_t *get_cpu(uint8_t cpu_id)
{
	if (cpu_id >= MAX_CPUS)
		return NULL;
	return &cpu_data[cpu_id];
}

void percpu_init_bsp(void)
{
	memset(cpu_data, 0, sizeof(cpu_data));

	uint8_t apic_id = lapic_get_id();

	cpu_data[0].apic_id = apic_id;
	cpu_data[0].cpu_id = 0;
	cpu_data[0].online = true;
	cpu_data[0].bsp = true;

	// BSP stack is already set up by bootloader
	cpu_data[0].stack_top = NULL; // Already using current stack
	cpu_data[0].stack_size = 0;

	num_cpus_online = 1;

	printk("BSP initialized: CPU 0 (APIC ID %u)\n", apic_id);
}

void percpu_init_ap(uint8_t apic_id)
{
	// Find free CPU slot
	uint8_t cpu_id = num_cpus_online;

	if (cpu_id >= MAX_CPUS)
	{
		// printk("ERROR: Too many CPUs (max %d)\n", MAX_CPUS);
		return;
	}

	cpu_data[cpu_id].apic_id = apic_id;
	cpu_data[cpu_id].cpu_id = cpu_id;
	cpu_data[cpu_id].online = true;
	cpu_data[cpu_id].bsp = false;

	__sync_fetch_and_add(&num_cpus_online, 1);

	// printk("AP initialized: CPU %u (APIC ID %u)\n", cpu_id, apic_id);
}
