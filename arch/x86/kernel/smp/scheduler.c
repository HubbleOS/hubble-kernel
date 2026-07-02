/**
 * @file scheduler.c
 * @brief SMP task scheduler implementation
 *
 * Implements a round-robin scheduler with per-CPU run queues,
 * task creation/destruction, context saving, and LAPIC timer dispatch.
 */

#include <mm/kmalloc.h>
#include <mm/pmm.h>
#include <mm/slab.h>
#include <mm/vmm.h>

#include <apic/apic.h>
#include <asm.h>
#include <gdt/gdt.h>
#include <hubble/printk.h>
#include <hubble/string.h>
#include <interrupt/interrupt.h>
#include <smp/smp.h>
#include <syscalls/syscall_entry.h>

#include "io.h"
#include "scheduler.h"
#include "task.h"

/* -- Constants ---------------------------------------------------------- */

#define CANARY 0xDEADBEEFCAFEBABEULL
#define MAX_PRIO 255
#define BASE_SLICE 5
#define USER_STACK_SIZE 0x10000

/* -- Static data -------------------------------------------------------- */

static task_t *current_task[MAX_CPUS];
static cpu_runqueue_t runqueues[MAX_CPUS];
static bool initialized = false;
static __thread bool dirty_queue = false;

volatile bool need_resched = false;

static slab_cache_t *fpu_cache = NULL;

static uint64_t next_user_stack = 0x6ff00000ULL;

/* -- Forward declarations ---------------------------------------------- */

static void task_state_load(task_t *task, registers_t *regs);
static uint64_t alloc_user_stack(void);

task_t *get_next_task(uint8_t cpu_id);
void scheduler_add_task(task_t *task);
void task_wrapper(void);
void schedule(registers_t *regs);
void save_context(task_t *current, registers_t *regs);
void free_context(task_t *task);

/* -- Scheduler state ---------------------------------------------------- */

/**
 * @brief Check if the scheduler has been initialized
 * @return true if initialized
 */
bool is_scheduler_initialized(void) { return initialized; }

/* -- Stack management --------------------------------------------------- */

/**
 * @brief Allocate a user stack region (descending from a fixed high address)
 * @return Base address of the allocated stack
 */
static uint64_t alloc_user_stack(void) {
  uint64_t base = next_user_stack;
  next_user_stack -= USER_STACK_SIZE + PAGE_SIZE;
  return base;
}

/* -- Task creation ------------------------------------------------------ */

/**
 * @brief Create a new task with an argument
 * @param entry_point Entry function pointer
 * @param entry_arg Argument passed to entry function
 * @param priority Task priority (0 = highest)
 * @param userspace true if this is a user-space task
 * @return Pointer to new task, or NULL on failure
 */
task_t *_task_create_with_arg(void (*entry_point)(void *), void *entry_arg,
                              uint32_t priority, bool userspace) {
  printk(KERN_INFO "Creating task\n");
  task_t *task = kmalloc(sizeof(task_t), GFP_KERNEL);
  if (!task)
    return NULL;

  memset(task, 0, sizeof(task_t));

  printk(KERN_INFO "Assigning PID\n");
  static uint32_t next_pid = 1;
  task->pid = __atomic_fetch_add(&next_pid, 1, __ATOMIC_SEQ_CST);

  printk(KERN_INFO "Setting state\n");
  task->state = TASK_READY;
  task->priority = priority;
  task->time_slice_max = BASE_SLICE + priority;
  task->time_slice = task->time_slice_max;
  task->context_saved = false;
  task->in_syscall = false;

  uint64_t cr3;
  asm volatile("mov %%cr3, %0" : "=r"(cr3));
  task->page_table = (uint64_t *)cr3;

  task->rsp0_size = 64 * 1024;
  task->rsp0 = (uint64_t)kmalloc(task->rsp0_size, GFP_KERNEL);
  if (!task->rsp0) {
    kfree(task);
    return NULL;
  }
  *(uint64_t *)(task->rsp0 + task->rsp0_size - 8) = CANARY;
  *(uint64_t *)(task->rsp0) = CANARY;

  if (!userspace) {
    printk(KERN_INFO "Allocating kernel task stack\n");
    task->stack_size = 16384;
    task->kernel_stack = (uint64_t)kmalloc(task->stack_size, GFP_KERNEL);
    if (!task->kernel_stack) {
      kfree((void *)task->rsp0);
      kfree(task);
      return NULL;
    }
  } else {
    task->kernel_stack = alloc_user_stack();
    task->stack_size = USER_STACK_SIZE;
  }

  printk(KERN_INFO "Setting up initial stack\n");
  uint64_t *stack_top =
      (uint64_t *)(((task->kernel_stack + task->stack_size) & ~0xFULL) - 8);

  printk(KERN_INFO "Initializing context\n");
  if (!userspace) {
    task->context.rip = (uint64_t)task_wrapper;
  } else {
    task->context.rip = (uint64_t)entry_point;
  }
  task->context.rsp = (uint64_t)stack_top;
  if (!userspace) {
    task->context.cs = 0x08;
    task->context.ss = 0x10;
    task->context.ds = 0x10;
    task->context.es = 0x10;
    task->context.fs = 0x10;
    task->context.gs = 0x10;
    task->context.rdi = (uint64_t)task;
  } else {
    task->context.cs = 0x23;
    task->context.ss = 0x1B;
    task->context.ds = 0x1B;
    task->context.es = 0x1B;
    task->context.fs = 0x1B;
    task->context.gs = 0x1B;
  }

  task->context.rflags = 0x202;
  task->entry_point = entry_point;
  task->entry_arg = entry_arg;

  printk(KERN_INFO "RSP: %p\n", task->context.rsp);
  printk(KERN_INFO "RIP: %p\n", task->context.rip);
  printk(KERN_INFO "CS: %p\n", task->context.cs);
  printk(KERN_INFO "SS: %p\n", task->context.ss);

  printk(KERN_INFO "Allocating FPU state\n");
  if (!fpu_cache)
    fpu_cache = slab_cache_create(512, 16);
  task->context.fpu_state = slab_cache_alloc(fpu_cache);

  if (task->context.fpu_state) {
    printk(KERN_INFO "Initializing FPU state\n");
    asm volatile("fxsave %0" : "=m"(*(char *)task->context.fpu_state));
  }

  task->signal = 0;
  printk(KERN_INFO "Task created\n");
  return task;
}

/**
 * @brief Map user stack pages into the task's page table
 * @param task Task whose stack to map
 * @param pml4_phys Physical address of target PML4
 */
void task_map_user_stack(task_t *task, uint64_t *pml4_phys) {
  uint64_t stack_base = task->kernel_stack;
  uint32_t size = task->stack_size;

  for (uint64_t i = 0; i < size; i += PAGE_SIZE) {
    uint64_t phys = pmm_alloc_page();
    if (!phys)
      return;

    vmm_map_page_into(pml4_phys, stack_base + i, phys,
                      PTE_PRESENT | PTE_USER | PTE_WRITE);
  }
}

/**
 * @brief Create a new task without an argument
 * @param entry_point Entry function pointer
 * @param priority Task priority (0 = highest)
 * @param userspace true if this is a user-space task
 * @return Pointer to new task, or NULL on failure
 */
task_t *_task_create_no_arg(void (*entry_point)(void), uint32_t priority,
                            bool userspace) {
  return _task_create_with_arg((void (*)(void *))entry_point, NULL, priority,
                               userspace);
}

/* -- Context management ------------------------------------------------ */

/**
 * @brief Load CPU register state from a task into the interrupt frame
 * @param task Task whose context to load
 * @param regs Register frame to populate
 */
static void task_state_load(task_t *task, registers_t *regs) {
  if (!task)
    return;
  regs->rax = task->context.rax;
  regs->rbx = task->context.rbx;
  regs->rcx = task->context.rcx;
  regs->rdx = task->context.rdx;
  regs->rsi = task->context.rsi;
  regs->rdi = task->context.rdi;
  regs->rbp = task->context.rbp;
  regs->rsp = task->context.rsp;
  regs->r8 = task->context.r8;
  regs->r9 = task->context.r9;
  regs->r10 = task->context.r10;
  regs->r11 = task->context.r11;
  regs->r12 = task->context.r12;
  regs->r13 = task->context.r13;
  regs->r14 = task->context.r14;
  regs->r15 = task->context.r15;
  regs->rip = task->context.rip;
  regs->rflags = task->context.rflags;
  regs->cs = (uint16_t)task->context.cs;
  regs->ss = (uint16_t)task->context.ss;
}

/**
 * @brief Save current CPU register state into a task's context
 * @param current Task whose context to save
 * @param regs Register frame to save from
 */
void save_context(task_t *current, registers_t *regs) {
  if (!current) {
    return;
  }
  if (current->context_saved) {
    return;
  }

  current->context.r15 = regs->r15;
  current->context.r14 = regs->r14;
  current->context.r13 = regs->r13;
  current->context.r12 = regs->r12;
  current->context.r11 = regs->r11;
  current->context.r10 = regs->r10;
  current->context.r9 = regs->r9;
  current->context.r8 = regs->r8;
  current->context.rbp = regs->rbp;
  current->context.rdi = regs->rdi;
  current->context.rsi = regs->rsi;
  current->context.rdx = regs->rdx;
  current->context.rcx = regs->rcx;
  current->context.rbx = regs->rbx;
  current->context.rax = regs->rax;

  current->context.rip = regs->rip;
  current->context.rsp = regs->rsp;
  current->context.rflags = regs->rflags;
  current->context.cs = regs->cs;
  current->context.ss = regs->ss;

  if (current->context.fpu_state) {
    asm volatile("fxsave (%0)" ::"r"(current->context.fpu_state) : "memory");
  }
}

/**
 * @brief Free resources associated with a task
 * @param task Task whose resources to free
 */
void free_context(task_t *task) {
  if (task->kernel_stack) {
    kfree((void *)task->kernel_stack);
  }

  if (task->context.fpu_state)
    slab_cache_free(fpu_cache, task->context.fpu_state);
}

/* -- Task lifecycle ----------------------------------------------------- */

/**
 * @brief Mark the current task as exited and trigger reschedule
 * @param exit_code Exit status code
 */
void task_exit(int exit_code) {
  uint8_t cpu_id = lapic_get_id();
  task_t *task = get_current_task();

  outb(0x3f8, 'E');

  if (!task)
    return;

  task->exit_code = exit_code;
  task->state = TASK_DEAD;

  spinlock_acquire(&runqueues[cpu_id].lock);

  size_t task_index = 0;
  for (size_t i = 0; i < runqueues[cpu_id].count; i++) {
    if (runqueues[cpu_id].queue[i] == task) {
      task_index = i;
      break;
    }
  }

  for (size_t i = task_index; i < runqueues[cpu_id].count - 1; i++) {
    runqueues[cpu_id].queue[i] = runqueues[cpu_id].queue[i + 1];
  }

  runqueues[cpu_id].count--;
  spinlock_release(&runqueues[cpu_id].lock);
  free_context(task);
  current_task[cpu_id] = NULL;

  asm volatile("int $32");

  while (1)
    hlt();
}

/**
 * @brief Put the current task to sleep (blocked state)
 */
void task_sleep(void) {
  task_t *current = get_current_task();
  if (!current)
    return;

  uint64_t user_rsp, user_rip, user_rflags;
  asm volatile("mov %%gs:8, %0" : "=r"(user_rsp));
  asm volatile("mov %%rcx,  %0" : "=r"(user_rip));
  asm volatile("mov %%r11,  %0" : "=r"(user_rflags));

  current->state = TASK_BLOCKED;
  current->time_slice = 0;

  if (current->in_syscall) {
    current->in_syscall_rsp = user_rsp;
    asm volatile("swapgs");
  }
  asm volatile("int $32");
  if (current->in_syscall)
    asm volatile("swapgs");
}

/**
 * @brief Wake a blocked task
 * @param task Task to wake
 */
void task_wake(task_t *task) {
  if (!task || task->state != TASK_BLOCKED)
    return;

  task->time_slice = task->time_slice_max;
  task->state = TASK_READY;
}

/* -- Task wrapper ------------------------------------------------------- */

/**
 * @brief Wrapper that calls the task's entry point and exits on return
 */
__attribute__((noreturn)) void task_wrapper(void) {
  register task_t *current asm("rdi");

  current->entry_point(current->entry_arg);

  task_exit(0);
  __builtin_unreachable();
}

/* -- Task termination --------------------------------------------------- */

/**
 * @brief Kill a task by task pointer
 * @param task Task to kill
 */
void task_kill_by_task(task_t *task) { task->state = TASK_ZOMBIE; }

/**
 * @brief Kill a task by PID
 * @param pid PID of task to kill
 */
void task_kill_by_pid(uint32_t pid) {}

/* -- Scheduling --------------------------------------------------------- */

/**
 * @brief Select the next task to run on a given CPU
 * @param cpu_id Logical CPU ID
 * @return Next task to run, or idle task if none available
 */
task_t *get_next_task(uint8_t cpu_id) {
  cpu_runqueue_t *rq = &runqueues[cpu_id];

  spinlock_acquire(&rq->lock);

  if (rq->count == 0) {
    spinlock_release(&rq->lock);
    return rq->idle_task;
  }

  for (size_t i = 0; i < rq->count; i++) {
    size_t index = (rq->next_index + i) % rq->count;
    if (rq->queue[index]->state == TASK_READY ||
        rq->queue[index]->state == TASK_ZOMBIE) {
      rq->next_index = (index + 1) % rq->count;
      task_t *next = rq->queue[index];
      spinlock_release(&rq->lock);
      return next;
    }
  }

  spinlock_release(&rq->lock);
  return rq->idle_task;
}

/**
 * @brief Get the currently running task on this CPU
 * @return Pointer to current task, or NULL if none
 */
task_t *get_current_task(void) {
  uint8_t cpu_id = lapic_get_id();
  return current_task[cpu_id];
}

/**
 * @brief Switch to the next task (called from timer interrupt)
 * @param regs Register state at interrupt time
 */
void schedule(registers_t *regs) {
  uint8_t cpu_id = lapic_get_id();
  task_t *old_task = get_current_task();
  task_t *new_task = get_next_task(cpu_id);

  if (!new_task || new_task == old_task)
    return;

  if (new_task && new_task == runqueues[cpu_id].idle_task) {
    if (old_task && old_task->state == TASK_RUNNING) {
      return;
    }
  }

  if (old_task && old_task->state == TASK_RUNNING)
    old_task->state = TASK_READY;

  new_task->state = TASK_RUNNING;
  new_task->cpu = cpu_id;
  current_task[cpu_id] = new_task;

  extern cpu_local_t cpu_locals[];

  if (old_task && old_task->in_syscall && !old_task->in_syscall_rsp) {
    uint64_t user_rsp;
    asm volatile("mov %%gs:8, %0" : "=r"(user_rsp));
    old_task->in_syscall_rsp = user_rsp;
  }

  if (new_task->in_syscall) {
    cpu_locals[cpu_id].cpu_id = new_task->in_syscall_rsp;
  }
  cpu_locals[cpu_id].rsp0 = new_task->rsp0 + new_task->rsp0_size;
  tss_set_rsp0(new_task->rsp0 + new_task->rsp0_size);

  if (new_task->page_table != (old_task ? old_task->page_table : NULL)) {
    asm volatile("mov %0, %%cr3" ::"r"(new_task->page_table) : "memory");
  }
  task_state_load(new_task, regs);
}

/* -- Timer handler ------------------------------------------------------ */

/**
 * @brief LAPIC timer interrupt handler
 * @param regs Register state at interrupt time
 *
 * Saves context, decrements time slice, and schedules next task when
 * the current task's slice expires.
 */
void lapic_timer_handler(registers_t *regs) {
  if (!initialized) {
    return;
  }
  task_t *current = get_current_task();

  if (current) {
    save_context(current, regs);

    if (current->time_slice > 0)
      current->time_slice--;
  }

  if (current && current->rsp0) {
    uint64_t *canary = (uint64_t *)(current->rsp0);
    if (*canary != CANARY) {
      printk(KERN_ERR "STACK UNDERFLOW on task pid=%d, data=0x%llx\n",
             current->pid, *(uint64_t *)(current->rsp0));
      *(uint64_t *)(current->rsp0) = CANARY;
    }
  }

  if (!current || current->time_slice <= 0) {
    if (current) {
      current->time_slice = current->time_slice_max;
    }

    schedule(regs);
  }
  current = get_current_task();

  if (!current) {
    while (1) {
      asm volatile("pause");
      asm volatile("hlt");
    }

    return;
  }

  if (current->state == TASK_ZOMBIE) {
    if (current->spinlocks > 0) {
      return;
    }
    task_exit(-1);
  }

  lapic_eoi();
}

/* -- Idle task ---------------------------------------------------------- */

/**
 * @brief Idle task that runs when no other task is ready
 */
void idle_task(void) {
  uint16_t count = 0;
  while (1) {
    hlt();
  }
}

/* -- Initialization ----------------------------------------------------- */

/**
 * @brief Initialize the scheduler and create idle tasks for each CPU
 */
void scheduler_init(void) {
  printk(KERN_INFO "Initializing scheduler\n");
  for (int i = 0; i < smp_get_cpu_count(); i++) {
    printk(KERN_INFO "Initializing runqueue for CPU %d\n", i);
    runqueues[i].count = 0;
    runqueues[i].next_index = 0;
    task_t *idle = task_create(idle_task, 255, 0);
    runqueues[i].idle_task = idle;
    current_task[i] = NULL;
  }

  extern void kmain_thread(void);
  task_t *kmain = task_create(kmain_thread, 255, 0);
  scheduler_add_task(kmain);

  initialized = true;
  while (1) {
    hlt();
  }
}

/**
 * @brief Add a task to the least-loaded CPU's run queue
 * @param task Task to add
 */
void scheduler_add_task(task_t *task) {
  int target_cpu = 0;
  size_t min_load = runqueues[0].count;

  for (int i = 1; i < smp_get_cpu_count(); i++) {
    if (runqueues[i].count < min_load) {
      min_load = runqueues[i].count;
      target_cpu = i;
    }
  }

  spinlock_acquire(&runqueues[target_cpu].lock);
  runqueues[target_cpu].queue[runqueues[target_cpu].count++] = task;
  task->cpu = target_cpu;
  spinlock_release(&runqueues[target_cpu].lock);
}
