//! Signals: each task's signal state, sending, choosing what to do on the
//! way back to user space, and the signal syscalls.
//!
//! The state lives inside task_t (C reserves TASK_SIGNALS_SIZE bytes, see
//! hubble/signal.h); C hands us pointers to it. Anything may send a signal
//! at any time, from any CPU or an interrupt handler: sending only touches
//! `pending` and the fields under `lock`. Everything else (handlers, the
//! blocked mask) is only ever changed by the task itself.

use core::ffi::c_void;
use core::sync::atomic::{AtomicU64, Ordering};

use crate::kernel::*;
use crate::sigframe;

pub const NSIG: usize = 64;

pub const SIGILL: i32 = 4;
pub const SIGTRAP: i32 = 5;
pub const SIGBUS: i32 = 7;
pub const SIGFPE: i32 = 8;
pub const SIGKILL: i32 = 9;
pub const SIGSEGV: i32 = 11;
pub const SIGCHLD: i32 = 17;
pub const SIGCONT: i32 = 18;
pub const SIGSTOP: i32 = 19;
pub const SIGTSTP: i32 = 20;
pub const SIGTTIN: i32 = 21;
pub const SIGTTOU: i32 = 22;
pub const SIGURG: i32 = 23;
pub const SIGWINCH: i32 = 28;
pub const SIGSYS: i32 = 31;

pub const SIG_DFL: u64 = 0;
pub const SIG_IGN: u64 = 1;

pub const SA_NOCLDSTOP: u64 = 0x1;
pub const SA_RESTORER: u64 = 0x0400_0000;
pub const SA_RESTART: u64 = 0x1000_0000;
pub const SA_NODEFER: u64 = 0x4000_0000;
pub const SA_RESETHAND: u64 = 0x8000_0000;

const fn bit(sig: i32) -> u64 {
    1 << (sig - 1)
}

/// What no handler or mask can change.
const UNBLOCKABLE: u64 = bit(SIGKILL) | bit(SIGSTOP);
const STOP_SIGNALS: u64 = bit(SIGSTOP) | bit(SIGTSTP) | bit(SIGTTIN) | bit(SIGTTOU);
/// Raised by the faulting instruction itself: handled before the rest.
const SYNC_SIGNALS: u64 =
    bit(SIGSEGV) | bit(SIGBUS) | bit(SIGILL) | bit(SIGTRAP) | bit(SIGFPE) | bit(SIGSYS);

fn valid(sig: i32) -> bool {
    (1..=NSIG as i32).contains(&sig)
}

/// What a signal does when nobody installed a handler.
#[derive(PartialEq)]
enum Default {
    Terminate,
    Ignore,
    Stop,
    Continue,
}

fn default_action(sig: i32) -> Default {
    match sig {
        SIGCHLD | SIGURG | SIGWINCH => Default::Ignore,
        SIGCONT => Default::Continue,
        SIGSTOP | SIGTSTP | SIGTTIN | SIGTTOU => Default::Stop,
        // Everything else, real-time signals included, terminates. The
        // "core" ones (SIGQUIT, SIGSEGV, ...) too: there are no core dumps.
        _ => Default::Terminate,
    }
}

/// struct sigaction as rt_sigaction takes it on x86_64.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct SigAction {
    pub handler: u64,
    pub flags: u64,
    pub restorer: u64,
    pub mask: u64,
}

const DEFAULT_ACTION: SigAction = SigAction {
    handler: SIG_DFL,
    flags: 0,
    restorer: 0,
    mask: 0,
};

/// What the sender tells the receiver (C: struct sig_info). Standard
/// signals don't queue, so one per signal number is kept.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct SigInfo {
    pub code: i32,   // si_code: SI_USER, CLD_EXITED, SEGV_MAPERR, ...
    pub pid: i32,    // sender (kill), or the child (SIGCHLD)
    pub status: i32, // SIGCHLD: exit code or signal
    pub _pad: i32,
    pub addr: u64, // faults: the address
}

const NO_INFO: SigInfo = SigInfo {
    code: 0,
    pid: 0,
    status: 0,
    _pad: 0,
    addr: 0,
};

/// Stop/continue news for the parent's wait4(WUNTRACED / WCONTINUED).
const REPORT_NONE: u32 = 0;
const REPORT_STOPPED: u32 = 1;
const REPORT_CONTINUED: u32 = 2;

pub const WUNTRACED: i32 = 2;
pub const WCONTINUED: i32 = 8;

#[repr(C)]
pub struct SigState {
    /// Guards `info`, `stop_sig` and `report` (and pending's changes that
    /// go with them).
    lock: IrqLock,
    pending: AtomicU64,
    pub blocked: u64,
    /// sigsuspend's caller mask, put back once a handler has run.
    saved_mask: u64,
    has_saved_mask: bool,
    /// The signal that stopped the task; 0 while it runs.
    stop_sig: u32,
    report: u32,
    /// What restart_syscall continues: the deadline (HPET ns) of the sleep
    /// that returned -ERESTART_RESTARTBLOCK.
    restart_deadline: u64,
    pub actions: [SigAction; NSIG],
    info: [SigInfo; NSIG],
}

/// Room C keeps for a SigState in task_t: TASK_SIGNALS_WORDS in
/// hubble/signal.h. Raise both together.
const TASK_SIGNALS_SIZE: usize = 464 * 8;
const _: () = assert!(core::mem::size_of::<SigState>() <= TASK_SIGNALS_SIZE);
const _: () = assert!(core::mem::align_of::<SigState>() <= 8);

impl SigState {
    fn action(&self, sig: i32) -> &SigAction {
        &self.actions[sig as usize - 1]
    }

    /// Whether delivering `sig` right now would do nothing at all.
    fn ignored(&self, sig: i32) -> bool {
        match self.action(sig).handler {
            SIG_IGN => true,
            SIG_DFL => matches!(default_action(sig), Default::Ignore | Default::Continue),
            _ => false,
        }
    }

    /// Pending signals that would do something if delivered now.
    fn deliverable(&self) -> u64 {
        let mut set = self.pending.load(Ordering::Acquire) & (!self.blocked | UNBLOCKABLE);
        let mut out = 0;
        while set != 0 {
            let sig = set.trailing_zeros() as i32 + 1;
            set &= set - 1;
            if !self.ignored(sig) {
                out |= bit(sig);
            }
        }
        out
    }

    /// Take `sig` off the pending set, with what its sender said.
    fn take(&mut self, sig: i32) -> SigInfo {
        let _g = self.lock.lock();
        self.pending.fetch_and(!bit(sig), Ordering::AcqRel);
        core::mem::replace(&mut self.info[sig as usize - 1], NO_INFO)
    }

    fn set_blocked(&mut self, mask: u64) {
        self.blocked = mask & !UNBLOCKABLE;
    }
}

/// # Safety: `p` points at a task's signal state (C keeps it alive).
unsafe fn state<'a>(p: *mut c_void) -> &'a mut SigState {
    unsafe { &mut *(p as *mut SigState) }
}

fn current<'a>() -> &'a mut SigState {
    unsafe { state(signal_current()) }
}

/* -- Called from C ------------------------------------------------------ */

/// Mark `sig` pending for the task owning `s`; the caller wakes it.
/// SIGKILL and SIGCONT also end a stop, and a stop signal cancels a
/// pending SIGCONT (and the other way round), as on Linux.
#[unsafe(no_mangle)]
pub extern "C" fn sig_queue(s: *mut c_void, sig: i32, info: *const SigInfo) {
    let s = unsafe { state(s) };
    if !valid(sig) {
        return;
    }
    let _g = s.lock.lock();
    if sig == SIGCONT || sig == SIGKILL {
        s.pending.fetch_and(!STOP_SIGNALS, Ordering::AcqRel);
        if s.stop_sig != 0 {
            s.stop_sig = 0;
            if sig == SIGCONT {
                s.report = REPORT_CONTINUED;
            }
        }
    } else if bit(sig) & STOP_SIGNALS != 0 {
        s.pending.fetch_and(!bit(SIGCONT), Ordering::AcqRel);
    }
    s.info[sig as usize - 1] = if info.is_null() { NO_INFO } else { unsafe { *info } };
    s.pending.fetch_or(bit(sig), Ordering::AcqRel);
}

/// A fault's signal: it can't be blocked or ignored (the instruction
/// would just fault again), so those go back to the default first.
#[unsafe(no_mangle)]
pub extern "C" fn sig_force(s: *mut c_void, sig: i32, info: *const SigInfo) {
    let st = unsafe { state(s) };
    if !valid(sig) {
        return;
    }
    if st.blocked & bit(sig) != 0 || st.action(sig).handler == SIG_IGN {
        st.blocked &= !bit(sig);
        st.actions[sig as usize - 1] = DEFAULT_ACTION;
    }
    sig_queue(s, sig, info);
}

/// Whether a signal is waiting that should interrupt a blocking call.
#[unsafe(no_mangle)]
pub extern "C" fn sig_pending(s: *mut c_void) -> bool {
    unsafe { state(s) }.deliverable() != 0
}

/// Whether the task is stopped (and not killed meanwhile).
#[unsafe(no_mangle)]
pub extern "C" fn sig_stopped(s: *mut c_void) -> bool {
    let s = unsafe { state(s) };
    s.stop_sig != 0 && s.pending.load(Ordering::Acquire) & bit(SIGKILL) == 0
}

/// Whether the parent asked not to hear about stops (SA_NOCLDSTOP).
#[unsafe(no_mangle)]
pub extern "C" fn sig_nocldstop(parent: *mut c_void) -> bool {
    unsafe { state(parent) }.action(SIGCHLD).flags & SA_NOCLDSTOP != 0
}

/// fork(): the child inherits handlers and the mask, nothing pending.
#[unsafe(no_mangle)]
pub extern "C" fn sig_fork(child: *mut c_void, parent: *mut c_void) {
    let (c, p) = unsafe { (state(child), state(parent)) };
    c.blocked = p.blocked;
    c.actions = p.actions;
}

/// execve(): handlers point into the old program, so they go back to the
/// default; ignored signals stay ignored, the mask and pending stay.
#[unsafe(no_mangle)]
pub extern "C" fn sig_exec(s: *mut c_void) {
    let s = unsafe { state(s) };
    for a in s.actions.iter_mut() {
        if a.handler != SIG_IGN {
            *a = DEFAULT_ACTION;
        }
    }
}

/// wait4 on a child: its stop or continue, as a wait status, if there is
/// news that `options` asks for. `take` consumes it.
#[unsafe(no_mangle)]
pub extern "C" fn sig_report(child: *mut c_void, options: i32, take: bool) -> i32 {
    let s = unsafe { state(child) };
    let _g = s.lock.lock();
    let status = match s.report {
        REPORT_STOPPED if options & WUNTRACED != 0 && s.stop_sig != 0 => {
            0x7f | (s.stop_sig as i32) << 8
        }
        REPORT_CONTINUED if options & WCONTINUED != 0 => 0xffff,
        _ => return 0,
    };
    if take {
        s.report = REPORT_NONE;
    }
    status
}

/// On the way back to user space: act on the next pending signal.
///
/// Returns 0 to go on to user space (maybe into a handler: `regs` then
/// points there), a signal number when the task must die of it, or minus
/// a signal number when it must stop (C stops it and calls again).
/// `from_syscall`: `regs` ends a syscall whose result is in rax, which
/// may say "interrupted, restart me".
#[unsafe(no_mangle)]
pub extern "C" fn sig_next(s: *mut c_void, regs: *mut Regs, from_syscall: bool) -> i32 {
    let s = unsafe { state(s) };
    let regs = unsafe { &mut *regs };

    if s.pending.load(Ordering::Acquire) == 0 && !s.has_saved_mask {
        restart_if_interrupted(regs, from_syscall);
        return 0;
    }

    loop {
        let set = s.pending.load(Ordering::Acquire) & (!s.blocked | UNBLOCKABLE);
        if set == 0 {
            break;
        }
        let pick = if set & bit(SIGKILL) != 0 {
            bit(SIGKILL)
        } else if set & SYNC_SIGNALS != 0 {
            set & SYNC_SIGNALS
        } else {
            set
        };
        let sig = pick.trailing_zeros() as i32 + 1;
        let info = s.take(sig);

        if sig == SIGKILL {
            return SIGKILL;
        }
        let action = *s.action(sig);
        match action.handler {
            SIG_IGN => continue,
            SIG_DFL => match default_action(sig) {
                Default::Ignore | Default::Continue => continue,
                Default::Terminate => return sig,
                Default::Stop => {
                    let _g = s.lock.lock();
                    s.stop_sig = sig as u32;
                    s.report = REPORT_STOPPED;
                    return -sig;
                }
            },
            _ => {
                if from_syscall {
                    match regs.rax as i64 {
                        e if e == -ERESTARTSYS && action.flags & SA_RESTART != 0 => {
                            restart(regs)
                        }
                        e if e == -ERESTARTSYS || e == -ERESTART_RESTARTBLOCK => {
                            regs.rax = -EINTR as u64
                        }
                        _ => {}
                    }
                }
                // The mask to go back to once the handler returns.
                let old_mask = if s.has_saved_mask {
                    s.has_saved_mask = false;
                    s.saved_mask
                } else {
                    s.blocked
                };
                if !sigframe::setup(regs, sig, &action, &info, old_mask) {
                    return SIGSEGV; // no room for the frame on its stack
                }
                let mut mask = s.blocked | action.mask;
                if action.flags & SA_NODEFER == 0 {
                    mask |= bit(sig);
                }
                s.set_blocked(mask);
                if action.flags & SA_RESETHAND != 0 {
                    s.actions[sig as usize - 1] = DEFAULT_ACTION;
                }
                return 0;
            }
        }
    }

    // No handler ran: an interrupted call just goes on.
    if s.has_saved_mask {
        s.has_saved_mask = false;
        s.blocked = s.saved_mask;
    }
    restart_if_interrupted(regs, from_syscall);
    0
}

/// Run the syscall again: `syscall` (and `int 0x80`) is two bytes long.
fn restart(regs: &mut Regs) {
    regs.rax = regs.int_no;
    regs.rip -= 2;
}

fn restart_if_interrupted(regs: &mut Regs, from_syscall: bool) {
    if !from_syscall {
        return;
    }
    match regs.rax as i64 {
        e if e == -ERESTARTSYS => restart(regs),
        e if e == -ERESTART_RESTARTBLOCK => {
            regs.int_no = SYS_RESTART_SYSCALL;
            restart(regs);
        }
        _ => {}
    }
}

/// A sleep about to return -ERESTART_RESTARTBLOCK: what restart_syscall
/// waits for if it is continued.
#[unsafe(no_mangle)]
pub extern "C" fn sig_set_restart(s: *mut c_void, deadline_ns: u64) {
    unsafe { state(s) }.restart_deadline = deadline_ns;
}

#[unsafe(no_mangle)]
pub extern "C" fn sig_restart(s: *mut c_void) -> u64 {
    unsafe { state(s) }.restart_deadline
}

/* -- Syscalls ------------------------------------------------------------ */

const SIG_BLOCK: i32 = 0;
const SIG_UNBLOCK: i32 = 1;
const SIG_SETMASK: i32 = 2;

/// The kernel sigset_t is 64 bits; anything else is a wrong ABI.
const SIGSET_SIZE: usize = 8;

#[unsafe(no_mangle)]
pub extern "C" fn sys_rt_sigaction(
    sig: i32,
    act: *const SigAction,
    oact: *mut SigAction,
    size: usize,
) -> i64 {
    if !valid(sig) || size != SIGSET_SIZE {
        return -EINVAL;
    }
    let s = current();
    let new = if act.is_null() {
        None
    } else if bit(sig) & UNBLOCKABLE != 0 {
        return -EINVAL;
    } else {
        match copy_from_user::<SigAction>(act as u64) {
            Some(a) => Some(a),
            None => return -EFAULT,
        }
    };
    if !oact.is_null() && !copy_to_user(oact as u64, s.action(sig)) {
        return -EFAULT;
    }
    if let Some(mut a) = new {
        a.mask &= !UNBLOCKABLE;
        s.actions[sig as usize - 1] = a;
        // POSIX: setting a pending signal to be ignored discards it.
        if s.ignored(sig) {
            s.take(sig);
        }
    }
    0
}

#[unsafe(no_mangle)]
pub extern "C" fn sys_rt_sigprocmask(how: i32, set: *const u64, oset: *mut u64, size: usize) -> i64 {
    if size != SIGSET_SIZE {
        return -EINVAL;
    }
    let s = current();
    let old = s.blocked;
    if !set.is_null() {
        let Some(set) = copy_from_user::<u64>(set as u64) else {
            return -EFAULT;
        };
        let mask = match how {
            SIG_BLOCK => old | set,
            SIG_UNBLOCK => old & !set,
            SIG_SETMASK => set,
            _ => return -EINVAL,
        };
        s.set_blocked(mask);
    }
    if !oset.is_null() && !copy_to_user(oset as u64, &old) {
        return -EFAULT;
    }
    0
}

#[unsafe(no_mangle)]
pub extern "C" fn sys_rt_sigpending(set: *mut u64, size: usize) -> i64 {
    if size != SIGSET_SIZE {
        return -EINVAL;
    }
    let s = current();
    let pending = s.pending.load(Ordering::Acquire) & s.blocked;
    if copy_to_user(set as u64, &pending) { 0 } else { -EFAULT }
}

extern "C" fn has_deliverable(s: *mut c_void) -> bool {
    sig_pending(s)
}

/// Sleep with `mask` blocked until a signal that is not arrives; it is
/// delivered (its handler runs) before the old mask comes back.
#[unsafe(no_mangle)]
pub extern "C" fn sys_rt_sigsuspend(set: *const u64, size: usize) -> i64 {
    if size != SIGSET_SIZE {
        return -EINVAL;
    }
    let Some(mask) = copy_from_user::<u64>(set as u64) else {
        return -EFAULT;
    };
    let s = current();
    s.saved_mask = s.blocked;
    s.has_saved_mask = true;
    s.set_blocked(mask);
    let p = s as *mut SigState as *mut c_void;
    while unsafe { signal_wait(has_deliverable, p, 0) } != 0 {}
    -EINTR
}

/// ppoll, pselect6 and epoll_pwait: block `mask` for the call. The old
/// mask comes back on the way out to user space (after a handler runs, if
/// one does), as with sigsuspend.
#[unsafe(no_mangle)]
pub extern "C" fn sig_set_temp_mask(s: *mut c_void, mask: u64) {
    let s = unsafe { state(s) };
    if !s.has_saved_mask {
        s.saved_mask = s.blocked;
        s.has_saved_mask = true;
    }
    s.set_blocked(mask);
}

#[unsafe(no_mangle)]
pub extern "C" fn sys_pause() -> i64 {
    let p = current() as *mut SigState as *mut c_void;
    while unsafe { signal_wait(has_deliverable, p, 0) } != 0 {}
    -EINTR
}

struct TimedWait {
    s: *mut SigState,
    wanted: u64,
}

extern "C" fn timedwait_ready(arg: *mut c_void) -> bool {
    let w = unsafe { &*(arg as *const TimedWait) };
    let s = unsafe { &*w.s };
    s.pending.load(Ordering::Acquire) & w.wanted != 0 || s.deliverable() & !w.wanted != 0
}

/// Wait for one of `set` (normally blocked, so it stays pending) and take
/// it without running a handler. busybox init sleeps here for SIGCHLD.
#[unsafe(no_mangle)]
pub extern "C" fn sys_rt_sigtimedwait(
    set: *const u64,
    info: *mut c_void,
    timeout: *const Timespec,
    size: usize,
) -> i64 {
    if size != SIGSET_SIZE {
        return -EINVAL;
    }
    let Some(wanted) = copy_from_user::<u64>(set as u64) else {
        return -EFAULT;
    };
    let wanted = wanted & !UNBLOCKABLE;

    let deadline = if timeout.is_null() {
        0 // none
    } else {
        let Some(t) = copy_from_user::<Timespec>(timeout as u64) else {
            return -EFAULT;
        };
        if t.tv_sec < 0 || !(0..NS_PER_SEC as i64).contains(&t.tv_nsec) {
            return -EINVAL;
        }
        // A zero timeout polls: a deadline already past does that.
        let now = unsafe { hpet_get_time_ns() };
        now + t.tv_sec as u64 * NS_PER_SEC + t.tv_nsec as u64
    };

    let s = current();
    let mut w = TimedWait { s, wanted };
    let ready = unsafe { signal_wait(timedwait_ready, &mut w as *mut _ as *mut c_void, deadline) };

    let got = s.pending.load(Ordering::Acquire) & wanted;
    if got == 0 {
        // Timed out, or some other signal is to be handled first.
        return if ready == 0 { -EINTR } else { -EAGAIN };
    }
    let sig = got.trailing_zeros() as i32 + 1;
    let si = s.take(sig);
    if !info.is_null() && !copy_to_user(info as u64, &sigframe::siginfo(sig, &si)) {
        return -EFAULT;
    }
    sig as i64
}

/// rt_sigreturn: back from a handler to where the signal interrupted.
#[unsafe(no_mangle)]
pub extern "C" fn sys_rt_sigreturn(regs: *mut Regs) -> i64 {
    let regs = unsafe { &mut *regs };
    let s = current();
    match sigframe::restore(regs) {
        Some(mask) => {
            s.set_blocked(mask);
            regs.rax as i64
        }
        None => {
            // A trashed frame: nowhere sensible to return to.
            sig_force(s as *mut SigState as *mut c_void, SIGSEGV, core::ptr::null());
            0
        }
    }
}
