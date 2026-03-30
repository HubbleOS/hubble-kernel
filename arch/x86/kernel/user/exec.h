#pragma once

#include <smp/task.h>
#include <smp/scheduler.h>

#include <mm/vmm.h>
#include <mm/pmm.h>

task_t *exec(const char *path);
