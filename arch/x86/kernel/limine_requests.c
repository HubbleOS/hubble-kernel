/**
 * @file limine_requests.c
 * @brief Limine request definitions — the single source of truth
 *
 * All Limine protocol requests are defined here. The corresponding
 * declarations live in limine_requests.h. Each kernel subsystem
 * includes that header to access the responses it needs.
 */

#include <limine.h>
#include <limine_requests.h>

/* -- Limine base revision (0) ----------------------------------------- */

__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[3] = LIMINE_BASE_REVISION(0);

/* -- Request start/end markers ---------------------------------------- */

__attribute__((used, section(".limine_requests_start_marker")))
static volatile uint64_t limine_requests_start[4] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end_marker")))
static volatile uint64_t limine_requests_end[2] = LIMINE_REQUESTS_END_MARKER;

/* -- Memory map ------------------------------------------------------- */

__attribute__((used, section(".limine_requests")))
volatile struct limine_memmap_request limine_memmap_req = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0,
};

/* -- HHDM (Higher Half Direct Map) ------------------------------------ */

__attribute__((used, section(".limine_requests")))
volatile struct limine_hhdm_request limine_hhdm_req = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0,
};

/* -- Framebuffer ------------------------------------------------------ */

__attribute__((used, section(".limine_requests")))
volatile struct limine_framebuffer_request limine_framebuffer_req = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0,
};

/* -- ACPI RSDP -------------------------------------------------------- */

__attribute__((used, section(".limine_requests")))
volatile struct limine_rsdp_request limine_rsdp_req = {
    .id = LIMINE_RSDP_REQUEST_ID,
    .revision = 0,
};

/* -- Modules (initramfs) ---------------------------------------------- */

__attribute__((used, section(".limine_requests")))
volatile struct limine_module_request limine_module_req = {
    .id = LIMINE_MODULE_REQUEST_ID,
    .revision = 0,
};

/* -- Executable address ----------------------------------------------- */

__attribute__((used, section(".limine_requests")))
volatile struct limine_executable_address_request limine_exec_addr_req = {
    .id = LIMINE_EXECUTABLE_ADDRESS_REQUEST_ID,
    .revision = 0,
};

/* -- Bootloader info -------------------------------------------------- */

__attribute__((used, section(".limine_requests")))
volatile struct limine_bootloader_info_request limine_bootloader_info_req = {
    .id = LIMINE_BOOTLOADER_INFO_REQUEST_ID,
    .revision = 0,
};
