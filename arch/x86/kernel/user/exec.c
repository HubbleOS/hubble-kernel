/**
 * @file exec.c
 * @brief Exec system call — create a task from an ELF binary
 *
 * Implements exec(): creates a user page table, loads an ELF
 * binary, creates a task with the entry point, maps a user
 * stack, and returns the new task to the scheduler.
 */

#include <asm.h>
#include <hubble/printk.h>
#include <hubble/string.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <smp/scheduler.h>
#include <smp/task.h>
#include <user/elf.h>

#define AT_NULL   0
#define AT_RANDOM 25

/**
 * @brief Load an ELF binary and create a task for it
 *
 * @param path VFS path to the ELF executable
 * @return Pointer to the new task, or NULL on failure
 */
task_t *exec(const char *path) {
  uint64_t *pml4 = vmm_create_user_pagemap();

  elf_image_t *image = kmalloc(sizeof(elf_image_t), GFP_KERNEL);
  elf_load(path, image, pml4);

  task_t *task = task_create((void *)image->entry, 0, 1);
  printk("task->fs_base: %lx\n", task->fs_base);

  task->heap_end = image->initial_brk;
  task->heap_start = image->initial_brk;

  if (image->has_tls) {
    void *tls = kmalloc(image->tls_memsz, GFP_KERNEL);

    memset(tls, 0, image->tls_memsz);
    memcpy(tls, image->tls_init, image->tls_filesz);

    task->fs_base = (uint64_t)tls;
    printk("task->fs_base: %lx\n", task->fs_base);

    kfree(image->tls_init);
  } else {
    size_t tcb_size = 256;
    void *tcb = kmalloc(tcb_size, GFP_KERNEL);
    memset(tcb, 0, tcb_size);
    *(uint64_t *)tcb =
        (uint64_t)tcb; // self-pointer -- %fs:0 має вказувати сам на себе
    task->fs_base = (uint64_t)tcb;
    printk("no tls (minimal tcb) task->fs_base: %lx\n", task->fs_base);
  }

  task_map_user_stack(task, pml4);
  printk("task->context.rsp: %lx\n", task->context.rsp);
  uint64_t old_cr3 = get_cr3();
  set_cr3((uint64_t)pml4);
  printk("task->context.rsp: %lx\n", task->context.rsp);
  char *str1 = (char *)(task->context.rsp - 3);
  memcpy(str1, "sh", 3); // 5, щоб включити '\0'

  // char *str2 = str1 - 20;
  // memcpy(str2, " ", ); // 19, щоб включити '\0'

  uint64_t *sp = (uint64_t *)((uintptr_t)str1 & ~0xFULL);

  sp = (uint64_t *)((uint8_t *)sp - 16);
  uint8_t *random_bytes = (uint8_t *)sp;
  for (int i = 0; i < 16; i++)
    random_bytes[i] = (uint8_t)(i * 0x9E + 0x37);

  sp = (uint64_t *)((uintptr_t)sp & ~0xFULL);

  // Будуємо стек У ЗВОРОТНОМУ порядку (auxv -> envp -> argv -> argc)
  *--sp = 0;                          // AT_NULL value
  *--sp = AT_NULL;                    // AT_NULL type
  *--sp = (uint64_t)random_bytes;     // AT_RANDOM value
  *--sp = AT_RANDOM;                  // AT_RANDOM type

  *--sp = 0;                          // envp[0] (кінець envp, порожнє середовище)

  *--sp = 0;                          // argv[2] = NULL (кінець argv)
  *--sp = 0;             // argv[1] = "hello"
  *--sp = (uint64_t)str1;             // argv[0] = "echo"

  *--sp = 2;                          // argc = 2 (!!)

  task->context.rsp = (uint64_t)sp;



  set_cr3(old_cr3);

  uint32_t lo, hi;

  asm volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(0xC0000100));

  uint64_t fsbase = ((uint64_t)hi << 32) | lo;
  printk("FS.base = %p\n", fsbase);

  task->page_table = pml4;
  kfree(image);
  return task;
}
