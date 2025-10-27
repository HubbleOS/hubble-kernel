#pragma once

void run_cmdf(const char *fmt, ...);

void run_qemu(const char *iso, const char *arch, int mem);
void run_qemu_default(void);

void run_build(void);
void run_clean(void);
void run_run(void);
