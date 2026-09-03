/**
 * @file limine_requests.h
 * @brief Centralized Limine request declarations for the Hubble kernel
 *
 * ALL Limine requests used by the kernel are declared here as extern globals.
 * The corresponding definitions live in limine_requests.c.
 *
 * Each kernel subsystem includes this header to access the Limine responses
 * it needs. There is NO intermediate boot-info structure — subsystems
 * consume Limine responses directly.
 */

#pragma once

#include <limine.h>

/* Memory map */
extern volatile struct limine_memmap_request limine_memmap_req;

/* Higher Half Direct Map (HHDM) */
extern volatile struct limine_hhdm_request limine_hhdm_req;

/* Framebuffer */
extern volatile struct limine_framebuffer_request limine_framebuffer_req;

/* ACPI RSDP */
extern volatile struct limine_rsdp_request limine_rsdp_req;

/* Modules (used for initramfs) */
extern volatile struct limine_module_request limine_module_req;

/* Kernel physical/virtual addresses */
extern volatile struct limine_executable_address_request limine_exec_addr_req;

/* Bootloader info */
extern volatile struct limine_bootloader_info_request limine_bootloader_info_req;
