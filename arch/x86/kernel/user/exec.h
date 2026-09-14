#pragma once

#include <smp/scheduler.h>
#include <smp/task.h>

#include <mm/pmm.h>
#include <mm/vmm.h>
task_t *execv(const char *path, char *const argv[], char *const envp[]);
task_t *exec(const char *path);
