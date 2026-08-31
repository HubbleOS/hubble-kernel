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

#define AT_NULL 0
#define AT_RANDOM 25

uint64_t build_user_stack(uint64_t stack_top, char *const argv[],
                          char *const envp[]) {
  // 1. Порахувати кількість argv/envp елементів
  int argc = 0;
  if (argv)
    while (argv[argc])
      argc++;

  int envc = 0;
  if (envp)
    while (envp[envc])
      envc++;

  uint8_t *sp = (uint8_t *)stack_top;

  // 2. Скопіювати самі рядки (argv, потім envp), знизу вгору,
  //    і одразу запам'ятати адреси, куди їх поклали
  uint64_t *argv_ptrs = kmalloc(sizeof(uint64_t) * (argc + 1), GFP_KERNEL);
  uint64_t *envp_ptrs = kmalloc(sizeof(uint64_t) * (envc + 1), GFP_KERNEL);

  for (int i = 0; i < argc; i++) {
    size_t len = strlen(argv[i]) + 1; // +1 для '\0'
    sp -= len;
    memcpy(sp, argv[i], len);
    argv_ptrs[i] = (uint64_t)sp;
  }
  argv_ptrs[argc] = 0;

  for (int i = 0; i < envc; i++) {
    size_t len = strlen(envp[i]) + 1;
    sp -= len;
    memcpy(sp, envp[i], len);
    envp_ptrs[i] = (uint64_t)sp;
  }
  envp_ptrs[envc] = 0;

  // 3. Вирівняти перед AT_RANDOM
  sp = (uint8_t *)((uintptr_t)sp & ~0xFULL);
  sp -= 16;
  uint8_t *random_bytes = sp;
  for (int i = 0; i < 16; i++)
    random_bytes[i] = (uint8_t)(i * 0x9E + 0x37);

  // 4. Вирівняти основний sp
  uint64_t *usp = (uint64_t *)((uintptr_t)sp & ~0xFULL);

  // 5. auxv (у зворотному порядку -- останній push стає першим у пам'яті)
  *--usp = 0;                      // AT_NULL value
  *--usp = AT_NULL;                // AT_NULL type
  *--usp = (uint64_t)random_bytes; // AT_RANDOM value
  *--usp = AT_RANDOM;              // AT_RANDOM type

  // 6. envp[] масив вказівників (у зворотному порядку)
  *--usp = 0; // NULL термінатор envp
  for (int i = envc - 1; i >= 0; i--)
    *--usp = envp_ptrs[i];

  // 7. argv[] масив вказівників
  *--usp = 0; // NULL термінатор argv
  for (int i = argc - 1; i >= 0; i--)
    *--usp = argv_ptrs[i];

  // 8. argc
  *--usp = argc;

  kfree(argv_ptrs);
  kfree(envp_ptrs);

  return (uint64_t)usp;
}

/**
 * @brief Load an ELF binary and create a task for it
 *
 * @param path VFS path to the ELF executable
 * @return Pointer to the new task, or NULL on failure
 */
task_t *execv(const char *path, char *const argv[], char *const envp[]) {
  uint64_t *pml4 = vmm_create_user_pagemap();

  elf_image_t *image = kmalloc(sizeof(elf_image_t), GFP_KERNEL);
  elf_load(path, image, pml4);

  task_t *task = task_create((void *)image->entry, 200, 1);
  printk("task->fs_base: %lx\n", task->mm.fs_base);

  task->mm.heap_end = image->initial_brk;
  task->mm.heap_start = image->initial_brk;

  if (image->has_tls) {
    void *tls = kmalloc(image->tls_memsz, GFP_KERNEL);

    memset(tls, 0, image->tls_memsz);
    memcpy(tls, image->tls_init, image->tls_filesz);

    task->mm.fs_base = (uint64_t)tls;
    printk("task->fs_base: %lx\n", task->mm.fs_base);

    kfree(image->tls_init);
  } else {
    // size_t tcb_size = 256;
    // void *tcb = kmalloc(tcb_size, GFP_KERNEL);
    // memset(tcb, 0, tcb_size);
    // *(uint64_t *)tcb =
    //     (uint64_t)tcb; // self-pointer -- %fs:0 має вказувати сам на себе
    // task->fs_base = (uint64_t)tcb;
    printk("no tls\n");
  }

  task_map_user_stack(task, pml4);
  printk("task->exec.context.rsp: %lx\n", task->exec.context.rsp);

  uint64_t old_cr3 = get_cr3();
  set_cr3((uint64_t)pml4);

  char *default_argv[] = {(char *)path, NULL};
  char *default_envp[] = {"PATH=/usr/bin:/busy/", NULL};

  uint64_t new_sp =
      build_user_stack(task->exec.context.rsp, argv ? argv : default_argv,
                       envp ? envp : default_envp);

  task->exec.context.rsp = new_sp;

  set_cr3(old_cr3);

  uint32_t lo, hi;

  asm volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(0xC0000100));

  uint64_t fsbase = ((uint64_t)hi << 32) | lo;
  printk("FS.base = %p\n", fsbase);

  task->mm.page_table = pml4;
  kfree(image);
  return task;
}

task_t *exec(const char *path) {

  task_t *task = execv(path, NULL, NULL);
  return 0;
}
