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
#include <msr.h>

#include <gdt/gdt.h>
#include <hubble/printk.h>
#include <hubble/string.h>
#include <interrupt/interrupt.h>
#include <smp/smp.h>
#include <syscalls/syscall_entry.h>

#include "io.h"
#include "scheduler.h"
#include "task.h"
#include "waitqueue.h"

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

  task->child_wait = kmalloc(sizeof(wait_queue_t), GFP_KERNEL);
  if (!task->child_wait) {
    kfree(task);
    return NULL;
  }
  waitqueue_init(task->child_wait);

  printk(KERN_INFO "Assigning PID\n");
  static uint32_t next_pid = 1;
  task->id.pid = __atomic_fetch_add(&next_pid, 1, __ATOMIC_SEQ_CST);

  printk(KERN_INFO "Setting state\n");
  task->linkage.state = TASK_READY;
  task->sched.priority = priority;
  task->sched.time_slice_max = BASE_SLICE + priority;
  task->sched.time_slice = task->sched.time_slice_max;
  task->exec.context_saved = false;
  task->exec.in_syscall = false;
  task->stacks.userspace = userspace;
  uint64_t cr3;
  asm volatile("mov %%cr3, %0" : "=r"(cr3));
  task->mm.page_table = (uint64_t *)cr3;

  task->exec.rsp0_size = 64 * 1024;
  task->exec.rsp0 = (uint64_t)kmalloc(task->exec.rsp0_size, GFP_KERNEL);
  if (!task->exec.rsp0) {
    kfree(task);
    return NULL;
  }
  *(uint64_t *)(task->exec.rsp0 + task->exec.rsp0_size - 8) = CANARY;
  *(uint64_t *)(task->exec.rsp0) = CANARY;

  if (!userspace) {
    printk(KERN_INFO "Allocating kernel task stack\n");
    task->stacks.stack_size = 16384;
    task->stacks.kernel_stack =
        (uint64_t)kmalloc(task->stacks.stack_size, GFP_KERNEL);
    if (!task->stacks.kernel_stack) {
      kfree((void *)task->exec.rsp0);
      kfree(task);
      return NULL;
    }
  } else {
    task->stacks.kernel_stack = alloc_user_stack();
    task->stacks.stack_size = USER_STACK_SIZE;
  }

  printk(KERN_INFO "Setting up initial stack\n");
  uint64_t *stack_top =
      (uint64_t *)(((task->stacks.kernel_stack + task->stacks.stack_size) &
                    ~0xFULL) -
                   8);

  printk(KERN_INFO "Initializing context\n");
  if (!userspace) {
    task->exec.context.rip = (uint64_t)task_wrapper;
  } else {
    task->exec.context.rip = (uint64_t)entry_point;
  }
  task->exec.context.rsp = (uint64_t)stack_top;
  if (!userspace) {
    task->exec.context.cs = 0x08;
    task->exec.context.ss = 0x10;
    task->exec.context.ds = 0x10;
    task->exec.context.es = 0x10;
    task->exec.context.fs = 0x10;
    task->exec.context.gs = 0x10;
    task->exec.context.rdi = (uint64_t)task;
  } else {
    task->exec.context.cs = 0x23;
    task->exec.context.ss = 0x1B;
    task->exec.context.ds = 0x1B;
    task->exec.context.es = 0x1B;
    task->exec.context.fs = 0x1B;
    task->exec.context.gs = 0x1B;
  }

  task->exec.context.rflags = 0x202;
  task->entry.entry_point = entry_point;
  task->entry.entry_arg = entry_arg;

  printk(KERN_INFO "RSP: %p\n", task->exec.context.rsp);
  printk(KERN_INFO "RIP: %p\n", task->exec.context.rip);
  printk(KERN_INFO "CS: %p\n", task->exec.context.cs);
  printk(KERN_INFO "SS: %p\n", task->exec.context.ss);

  printk(KERN_INFO "Allocating FPU state\n");
  if (!fpu_cache)
    fpu_cache = slab_cache_create(512, 16);
  task->exec.context.fpu_state = slab_cache_alloc(fpu_cache);

  if (task->exec.context.fpu_state) {
    printk(KERN_INFO "Initializing FPU state\n");
    asm volatile("fxsave %0" : "=m"(*(char *)task->exec.context.fpu_state));
  }

  task->signals.signal = 0;
  printk(KERN_INFO "Task created\n");
  return task;
}

/**
 * @brief Map user stack pages into the task's page table
 * @param task Task whose stack to map
 * @param pml4_phys Physical address of target PML4
 */
void task_map_user_stack(task_t *task, uint64_t *pml4_phys) {
  uint64_t stack_base = task->stacks.kernel_stack;
  uint32_t size = task->stacks.stack_size;

  for (uint64_t i = 0; i < size; i += PAGE_SIZE) {
    uint64_t phys = pmm_alloc_page();
    if (!phys)
      return;

    vmm_map_page_into(pml4_phys, stack_base + i, phys,
                      PTE_PRESENT | PTE_USER | PTE_WRITE);
  }

  /* Register the stack as a VMA too, or it's invisible to anything that
   * walks the address space by VMA (mmap's free-range search, fork()'s COW
   * clone) even though it's really mapped. */
  if (!task->mm.vm_map)
    task->mm.vm_map = vm_map_create();
  if (task->mm.vm_map) {
    vm_area_t *vma = kmalloc(sizeof(vm_area_t), GFP_ZERO);
    if (vma) {
      vma->base = stack_base;
      vma->size = size;
      vma->type = VMA_ANONYMOUS;
      vma->flags = VM_READ | VM_WRITE;
      vm_insert_area(task->mm.vm_map, vma);
    }
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
  regs->rax = task->exec.context.rax;
  regs->rbx = task->exec.context.rbx;
  regs->rcx = task->exec.context.rcx;
  regs->rdx = task->exec.context.rdx;
  regs->rsi = task->exec.context.rsi;
  regs->rdi = task->exec.context.rdi;
  regs->rbp = task->exec.context.rbp;
  regs->rsp = task->exec.context.rsp;
  regs->r8 = task->exec.context.r8;
  regs->r9 = task->exec.context.r9;
  regs->r10 = task->exec.context.r10;
  regs->r11 = task->exec.context.r11;
  regs->r12 = task->exec.context.r12;
  regs->r13 = task->exec.context.r13;
  regs->r14 = task->exec.context.r14;
  regs->r15 = task->exec.context.r15;
  regs->rip = task->exec.context.rip;
  regs->rflags = task->exec.context.rflags;
  regs->cs = (uint16_t)task->exec.context.cs;
  regs->ss = (uint16_t)task->exec.context.ss;
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
  if (current->exec.context_saved) {
    return;
  }

  current->exec.context.r15 = regs->r15;
  current->exec.context.r14 = regs->r14;
  current->exec.context.r13 = regs->r13;
  current->exec.context.r12 = regs->r12;
  current->exec.context.r11 = regs->r11;
  current->exec.context.r10 = regs->r10;
  current->exec.context.r9 = regs->r9;
  current->exec.context.r8 = regs->r8;
  current->exec.context.rbp = regs->rbp;
  current->exec.context.rdi = regs->rdi;
  current->exec.context.rsi = regs->rsi;
  current->exec.context.rdx = regs->rdx;
  current->exec.context.rcx = regs->rcx;
  current->exec.context.rbx = regs->rbx;
  current->exec.context.rax = regs->rax;

  current->exec.context.rip = regs->rip;
  current->exec.context.rsp = regs->rsp;
  current->exec.context.rflags = regs->rflags;
  current->exec.context.cs = regs->cs;
  current->exec.context.ss = regs->ss;

  if (current->exec.context.fpu_state) {
    asm volatile("fxsave (%0)" ::"r"(current->exec.context.fpu_state)
                 : "memory");
  }
}

/**
 * @brief Free resources associated with a task
 * @param task Task whose resources to free
 */
void free_context(task_t *task) {
  if (task->stacks.kernel_stack) {
    if (task->stacks.userspace) {
      // Розмапити і звільнити фізичні сторінки user-стека
      for (uint64_t i = 0; i < task->stacks.stack_size; i += PAGE_SIZE) {
        uint64_t va = task->stacks.kernel_stack + i;
        uint64_t phys = vmm_get_phys_from(task->mm.page_table, va);
        if (phys) {
          vmm_unmap_page_from(task->mm.page_table, va);
          pmm_free_page(phys);
        }
      }
    } else {
      kfree((void *)task->stacks.kernel_stack);
    }
  }

  if (task->exec.context.fpu_state)
    slab_cache_free(fpu_cache, task->exec.context.fpu_state);
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
  printk("exited with code: %d", exit_code);
  if (!task)
    return;

  /* Captured now, used after free_context() below - see the comment
   * further down at the matching swapgs. */
  bool was_in_syscall = task->exec.in_syscall;

  task->linkage.exit_code = exit_code;
  task->linkage.state = TASK_DEAD;

  if (task->linkage.parent)
    waitqueue_wake_all(task->linkage.parent->child_wait);

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

  /* GS_BASE is a single per-CPU MSR, not saved/restored per task. If we
   * got here mid-syscall (exit_group, the common case), syscall_entry's
   * own entry swapgs already swapped it to this cpu's real per-cpu
   * pointer and never swapped it back (we're not returning to
   * userspace, we're dying) - left as is, whichever task schedule()
   * picks next inherits that wrong value the moment IT tries its own
   * syscall_entry swapgs, exchanging against our leftover instead of
   * the real baseline. Swap back to the baseline ourselves before
   * yielding the CPU, exactly like task_sleep() does. Not needed if we
   * got here via task_wrapper() instead (a kernel task's entry point
   * just returning) - that path never swapped anything in the first
   * place. We never come back, so there's no matching swap-in. */
  if (was_in_syscall)
    asm volatile("swapgs");

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

  current->linkage.state = TASK_BLOCKED;
  current->sched.time_slice = 0;

  /* GS_BASE is a single per-CPU MSR, not saved/restored per task: while
   * we're blocked, some OTHER task's syscall can run on this CPU, and
   * ITS entry swapgs needs GS_BASE sitting at the "nobody's mid-syscall"
   * baseline (this cpu's real per-cpu pointer, stashed in KERNEL_GS_BASE)
   * to exchange against - not left at what WE had it as. So swap back to
   * that baseline before yielding the CPU, same as if we were actually
   * returning to userspace (conceptually, we're handing it to someone
   * else); swap back in once we actually resume. task_exit() mirrors the
   * swap-out half of this on its own way out, for the same reason. */
  if (current->exec.in_syscall) {
    current->exec.in_syscall_rsp = user_rsp;
    asm volatile("swapgs");
  }

  asm volatile("int $32");

  if (current->exec.in_syscall)
    asm volatile("swapgs");
}

/**
 * @brief Wake a blocked task
 * @param task Task to wake
 */
void task_wake(task_t *task) {
  if (!task || task->linkage.state != TASK_BLOCKED)
    return;

  task->sched.time_slice = task->sched.time_slice_max;
  task->linkage.state = TASK_READY;
}

/* -- Task wrapper ------------------------------------------------------- */

/**
 * @brief Wrapper that calls the task's entry point and exits on return
 */
__attribute__((noreturn)) void task_wrapper(void) {
  register task_t *current asm("rdi");

  current->entry.entry_point(current->entry.entry_arg);

  task_exit(0);
  __builtin_unreachable();
}

/* -- Task termination --------------------------------------------------- */

/**
 * @brief Kill a task by task pointer
 * @param task Task to kill
 */
void task_kill_by_task(task_t *task) { task->linkage.state = TASK_ZOMBIE; }

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
    if (rq->queue[index]->linkage.state == TASK_READY ||
        rq->queue[index]->linkage.state == TASK_ZOMBIE) {
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
    if (old_task && old_task->linkage.state == TASK_RUNNING) {
      return;
    }
  }

  if (old_task && old_task->linkage.state == TASK_RUNNING)
    old_task->linkage.state = TASK_READY;

  new_task->linkage.state = TASK_RUNNING;
  new_task->sched.cpu = cpu_id;
  current_task[cpu_id] = new_task;

  extern cpu_local_t cpu_locals[];

  if (old_task && old_task->exec.in_syscall && !old_task->exec.in_syscall_rsp) {
    uint64_t user_rsp;
    asm volatile("mov %%gs:8, %0" : "=r"(user_rsp));
    old_task->exec.in_syscall_rsp = user_rsp;
  }

  if (new_task->exec.in_syscall) {
    cpu_locals[cpu_id].cpu_id = new_task->exec.in_syscall_rsp;
  }
  cpu_locals[cpu_id].rsp0 = new_task->exec.rsp0 + new_task->exec.rsp0_size;
  tss_set_rsp0(new_task->exec.rsp0 + new_task->exec.rsp0_size);

  if (new_task->mm.page_table != (old_task ? old_task->mm.page_table : NULL)) {
    asm volatile("mov %0, %%cr3" ::"r"(new_task->mm.page_table) : "memory");
  }

  if (new_task->mm.fs_base) {
    printk("load fs_base new_task->fs_base: %lx\n", new_task->mm.fs_base);
    wrmsr(0xC0000100, new_task->mm.fs_base);
    printk("load fs_base loaded\n");
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

    if (current->sched.time_slice > 0)
      current->sched.time_slice--;
  }

  if (current && current->exec.rsp0) {
    uint64_t *canary = (uint64_t *)(current->exec.rsp0);
    if (*canary != CANARY) {
      printk(KERN_ERR "STACK UNDERFLOW on task pid=%d, data=0x%llx\n",
             current->id.pid, *(uint64_t *)(current->exec.rsp0));
      *(uint64_t *)(current->exec.rsp0) = CANARY;
    }
  }

  if (!current || current->sched.time_slice <= 0) {
    if (current) {
      current->sched.time_slice = current->sched.time_slice_max;
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

  if (current->linkage.state == TASK_ZOMBIE) {
    if (current->linkage.spinlocks > 0) {
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
  task->sched.cpu = target_cpu;
  spinlock_release(&runqueues[target_cpu].lock);
}
