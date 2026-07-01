/**
 * @file scheduler.h
 * @brief SMP task scheduler interface
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <interrupt/interrupt.h>
#include <smp/spinlock.h>
#include <smp/task.h>

/* -- Constants ---------------------------------------------------------- */

#define MAX_TASKS 256
#define MAX_CPUS 16

/* -- Per-CPU run queue -------------------------------------------------- */

/**
 * @brief Per-CPU run queue structure
 */
typedef struct {
  task_t *queue[MAX_TASKS];
  size_t count;
  spinlock_t lock;
  task_t *current;
  uint64_t idle_time;
  task_t *idle_task;
  uint32_t next_index;
} cpu_runqueue_t;

/**
 * @brief Per-CPU kill queue for zombie reaping
 */
typedef struct {
  task_t *queue[MAX_TASKS];
  size_t count;
} cpu_killqueue_t;

/* -- Scheduler core ----------------------------------------------------- */

/**
 * @brief Handler for the LAPIC timer interrupt
 * @param regs Register state at interrupt time
 */
void lapic_timer_handler(registers_t *regs);

/**
 * @brief Initialize the scheduler and idle tasks
 */
void scheduler_init(void);

/**
 * @brief Get the currently running task on this CPU
 * @return Pointer to current task, or NULL if none
 */
task_t *get_current_task(void);

/**
 * @brief Add a task to the least-loaded run queue
 * @param task Task to add
 */
void scheduler_add_task(task_t *task);

/**
 * @brief Check if the scheduler has been initialized
 * @return true if initialized
 */
bool is_scheduler_initialized(void);

/**
 * @brief Mark the current task as exited and reschedule
 * @param exit_code Exit status code
 */
void task_exit(int exit_code);

/* -- Task scheduling helpers -------------------------------------------- */

/**
 * @brief Wake a blocked task
 * @param task Task to wake
 */
void task_wake(task_t *task);

/**
 * @brief Put the current task to sleep (blocked)
 */
void task_sleep(void);

/* -- Task creation ------------------------------------------------------ */

/**
 * @brief Create a new task with an argument
 * @param entry_point Entry function pointer (takes void*)
 * @param entry_arg Argument passed to entry function
 * @param priority Task priority (0 = highest)
 * @param userspace true if this is a user-space task
 * @return Pointer to new task, or NULL on failure
 */
task_t *_task_create_with_arg(void (*entry_point)(void *), void *entry_arg,
                              uint32_t priority, bool userspace);

/**
 * @brief Create a new task without an argument
 * @param entry_point Entry function pointer (takes void)
 * @param priority Task priority (0 = highest)
 * @param userspace true if this is a user-space task
 * @return Pointer to new task, or NULL on failure
 */
task_t *_task_create_no_arg(void (*entry_point)(void), uint32_t priority,
                            bool userspace);

/**
 * @brief Map user stack pages into the task's page table
 * @param task Task whose stack to map
 * @param pml4_phys Physical address of target PML4
 */
void task_map_user_stack(task_t *task, uint64_t *pml4_phys);

#define _task_create_select(_1, _2, _3, _4, NAME) NAME

#define task_create(...)                                                       \
  _task_create_select(__VA_ARGS__, _task_create_with_arg,                      \
                      _task_create_no_arg)(__VA_ARGS__)

/* -- Task termination --------------------------------------------------- */

/**
 * @brief Kill a task by task pointer
 * @param task Task to kill
 */
void task_kill_by_task(task_t *task);

/**
 * @brief Kill a task by PID
 * @param pid PID of task to kill
 */
void task_kill_by_pid(uint32_t pid);

#define task_kill(...)                                                         \
  _task_kill_select(__VA_ARGS__, task_kill_by_pid,                             \
                    task_kill_by_task)(__VA_ARGS__)
#define _task_kill_select(_1, _2, _3, NAME, ...) NAME
