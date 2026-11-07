/*
 * alarm, setitimer and getitimer: the real-time interval timer
 * (ITIMER_REAL), which sends SIGALRM when it runs out.
 *
 * Armed timers live in a table keyed by pid, and one kernel thread sleeps
 * until the earliest runs out. pids are never reused, so a timer whose
 * process is gone simply finds nobody to signal and is dropped; nothing
 * has to disarm timers on exit. A child does not inherit its parent's
 * timer (it has another pid), and exec keeps it, as on Linux.
 */

#include <hpet/hpet.h>
#include <hubble/errno.h>
#include <hubble/printk.h>
#include <hubble/signal.h>
#include <hubble/syscalls.h>

#include <smp/scheduler.h>
#include <smp/spinlock.h>
#include <smp/task.h>

#define ITIMER_REAL 0
#define ITIMER_MAX 128
#define ITIMER_THREAD_PRIORITY 10
#define NS_PER_USEC 1000ULL

struct kernel_timeval {
  int64_t tv_sec;
  int64_t tv_usec;
};

struct itimerval {
  struct kernel_timeval interval;
  struct kernel_timeval value;
};

struct itimer {
  uint32_t pid; /* 0: a free slot */
  uint64_t expires_ns;
  uint64_t interval_ns;
};

static struct itimer timers[ITIMER_MAX];
static spinlock_t lock;
static task_t *thread;

static uint64_t tv_to_ns(const struct kernel_timeval *tv) {
  return (uint64_t)tv->tv_sec * NS_PER_SEC + (uint64_t)tv->tv_usec * NS_PER_USEC;
}

static void ns_to_tv(uint64_t ns, struct kernel_timeval *tv) {
  tv->tv_sec = (int64_t)(ns / NS_PER_SEC);
  tv->tv_usec = (int64_t)(ns % NS_PER_SEC / NS_PER_USEC);
}

static void itimer_thread(void *arg) {
  (void)arg;
  for (;;) {
    /* Prepare first: an arm() between here and task_wait() wakes us,
     * and task_wait() then returns at once to look again. */
    task_prepare_wait();
    uint64_t now = hpet_get_time_ns(), next = 0;
    uint32_t due[ITIMER_MAX];
    int ndue = 0;

    spinlock_acquire(&lock);
    for (int i = 0; i < ITIMER_MAX; i++) {
      struct itimer *t = &timers[i];
      if (!t->pid)
        continue;
      if (now >= t->expires_ns) {
        due[ndue++] = t->pid;
        if (t->interval_ns)
          t->expires_ns = now + t->interval_ns;
        else
          t->pid = 0;
      }
      if (t->pid && (!next || t->expires_ns < next))
        next = t->expires_ns;
    }
    spinlock_release(&lock);

    if (ndue) {
      task_cancel_wait();
      for (int i = 0; i < ndue; i++) {
        if (signal_send_pid((int)due[i], SIGALRM) == -ESRCH) {
          /* The process is gone: its periodic timer goes too. */
          spinlock_acquire(&lock);
          for (int j = 0; j < ITIMER_MAX; j++)
            if (timers[j].pid == due[i])
              timers[j].pid = 0;
          spinlock_release(&lock);
        }
      }
      continue;
    }
    task_wait(next); /* 0: until something is armed */
  }
}

void itimer_init(void) {
  spinlock_init(&lock, "itimer");
  thread = _task_create_with_arg(itimer_thread, NULL, ITIMER_THREAD_PRIORITY,
                                 false);
  if (!thread) {
    printk(KERN_ERR "itimer: failed to start the timer thread\n");
    return;
  }
  scheduler_add_task(thread);
}

/* Replace the caller's timer with @p value / @p interval (value 0 disarms
 * it); what was left of the old one goes to @p old. */
static long itimer_set(uint64_t value, uint64_t interval,
                       struct itimerval *old) {
  if (!thread || !hpet_is_initialized())
    return -ENOSYS;
  uint32_t pid = get_current_task()->id.pid;
  uint64_t now = hpet_get_time_ns();
  struct itimer *slot = NULL, *free_slot = NULL;
  long ret = 0;

  spinlock_acquire(&lock);
  for (int i = 0; i < ITIMER_MAX; i++) {
    if (timers[i].pid == pid)
      slot = &timers[i];
    else if (!timers[i].pid && !free_slot)
      free_slot = &timers[i];
  }
  if (old) {
    uint64_t left = 0, ival = 0;
    if (slot) {
      /* A timer that ran out but has not been sent reads as 1us left, so
       * alarm() never reports 0 for an armed timer. */
      left = slot->expires_ns > now ? slot->expires_ns - now : NS_PER_USEC;
      ival = slot->interval_ns;
    }
    ns_to_tv(left, &old->value);
    ns_to_tv(ival, &old->interval);
  }
  if (!value) {
    if (slot)
      slot->pid = 0;
  } else {
    if (!slot)
      slot = free_slot;
    if (!slot) {
      ret = -EAGAIN;
    } else {
      slot->pid = pid;
      slot->expires_ns = now + value;
      slot->interval_ns = interval;
    }
  }
  spinlock_release(&lock);

  if (value && !ret) {
    task_wake(thread);
    scheduler_kick(thread);
  }
  return ret;
}

static bool tv_valid(const struct kernel_timeval *tv) {
  return tv->tv_sec >= 0 && tv->tv_usec >= 0 && tv->tv_usec < 1000000;
}

long sys_setitimer(int which, const struct itimerval *new,
                   struct itimerval *old) {
  if (which != ITIMER_REAL)
    return -EINVAL; /* ITIMER_VIRTUAL and _PROF: no CPU time accounting */
  struct itimerval v = {0};
  if (new) {
    v = *new;
    if (!tv_valid(&v.value) || !tv_valid(&v.interval))
      return -EINVAL;
  }
  struct itimerval o;
  long ret = itimer_set(tv_to_ns(&v.value), tv_to_ns(&v.interval), &o);
  if (!ret && old)
    *old = o;
  return ret;
}

long sys_getitimer(int which, struct itimerval *cur) {
  if (which != ITIMER_REAL)
    return -EINVAL;
  if (!cur)
    return -EFAULT;
  if (!thread || !hpet_is_initialized())
    return -ENOSYS;

  uint32_t pid = get_current_task()->id.pid;
  uint64_t now = hpet_get_time_ns();
  struct itimerval v = {0};
  spinlock_acquire(&lock);
  for (int i = 0; i < ITIMER_MAX; i++) {
    if (timers[i].pid != pid)
      continue;
    uint64_t e = timers[i].expires_ns;
    ns_to_tv(e > now ? e - now : NS_PER_USEC, &v.value);
    ns_to_tv(timers[i].interval_ns, &v.interval);
  }
  spinlock_release(&lock);
  *cur = v;
  return 0;
}

long sys_alarm(unsigned int seconds) {
  struct itimerval old;
  if (itimer_set((uint64_t)seconds * NS_PER_SEC, 0, &old))
    return 0;
  /* Seconds left, rounded to nearest, and at least 1 if one was armed. */
  long left = old.value.tv_sec + (old.value.tv_usec >= 500000);
  if (!left && old.value.tv_usec)
    left = 1;
  return left;
}
