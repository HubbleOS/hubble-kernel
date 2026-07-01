/**
 * @file percpu.c
 * @brief Per-CPU data initialization and access
 */

#include <apic/apic.h>
#include <hubble/printk.h>
#include <hubble/string.h>

#include "percpu.h"

/* -- Global data -------------------------------------------------------- */

cpu_info_t cpu_data[MAX_CPUS];
volatile uint32_t num_cpus_online = 0;

/* -- CPU info access ---------------------------------------------------- */

/**
 * @brief Get current CPU info structure using APIC ID
 * @return Pointer to current CPU's cpu_info_t, or NULL on failure
 */
cpu_info_t *get_current_cpu(void) {
  uint8_t apic_id = lapic_get_id();

  for (int i = 0; i < MAX_CPUS; i++) {
    if (cpu_data[i].online && cpu_data[i].apic_id == apic_id)
      return &cpu_data[i];
  }

  return NULL;
}

/**
 * @brief Get CPU info by logical CPU ID
 * @param cpu_id Logical CPU ID
 * @return Pointer to cpu_info_t, or NULL if out of range
 */
cpu_info_t *get_cpu(uint8_t cpu_id) {
  if (cpu_id >= MAX_CPUS)
    return NULL;
  return &cpu_data[cpu_id];
}

/* -- Initialization ----------------------------------------------------- */

/**
 * @brief Initialize per-CPU data for the Bootstrap Processor (BSP)
 */
void percpu_init_bsp(void) {
  memset(cpu_data, 0, sizeof(cpu_data));

  uint8_t apic_id = lapic_get_id();

  cpu_data[0].apic_id = apic_id;
  cpu_data[0].cpu_id = 0;
  cpu_data[0].online = true;
  cpu_data[0].bsp = true;

  cpu_data[0].stack_top = NULL;
  cpu_data[0].stack_size = 0;

  num_cpus_online = 1;

  printk(KERN_OK "BSP initialized: CPU 0 (APIC ID %u)\n", apic_id);
}

/**
 * @brief Initialize per-CPU data for an Application Processor (AP)
 * @param apic_id APIC ID of the AP being initialized
 */
void percpu_init_ap(uint8_t apic_id) {
  uint8_t cpu_id = num_cpus_online;

  if (cpu_id >= MAX_CPUS) {
    return;
  }

  cpu_data[cpu_id].apic_id = apic_id;
  cpu_data[cpu_id].cpu_id = cpu_id;
  cpu_data[cpu_id].online = true;
  cpu_data[cpu_id].bsp = false;

  __sync_fetch_and_add(&num_cpus_online, 1);
}
