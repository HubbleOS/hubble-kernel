/**
 * @file exec.c
 * @brief Exec system call — create a task from an ELF binary
 *
 * Implements exec(): creates a user page table, loads an ELF
 * binary, creates a task with the entry point, maps a user
 * stack, and returns the new task to the scheduler.
 */

#include <hubble/printk.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <smp/scheduler.h>
#include <smp/task.h>
#include <user/elf.h>

/**
 * @brief Load an ELF binary and create a task for it
 *
 * @param path VFS path to the ELF executable
 * @return Pointer to the new task, or NULL on failure
 */
task_t *exec(const char *path) {
  uint64_t *pml4 = vmm_create_user_pagemap();

  uint64_t entry;
  elf_load(path, &entry, (uint64_t *)pml4);

  task_t *task1 = task_create((void *)entry, 255, 1);

  task_map_user_stack(task1, (uint64_t *)pml4);

  task1->page_table = pml4;
  return task1;
}
