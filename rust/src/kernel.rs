//! What this crate uses of the C kernel: the interrupt frame, a few
//! functions, errno values and checked access to user memory.

use core::ffi::{c_char, c_void};

/// registers_t (arch/x86/kernel/interrupt/interrupt.h): the frame every
/// interrupt, exception and syscall entry pushes. For a syscall, int_no
/// holds the syscall number (to restart it) and rip/rflags/rsp the user's.
#[repr(C)]
pub struct Regs {
    pub r15: u64,
    pub r14: u64,
    pub r13: u64,
    pub r12: u64,
    pub r11: u64,
    pub r10: u64,
    pub r9: u64,
    pub r8: u64,
    pub rbp: u64,
    pub rdi: u64,
    pub rsi: u64,
    pub rdx: u64,
    pub rcx: u64,
    pub rbx: u64,
    pub rax: u64,
    pub int_no: u64,
    pub err_code: u64,
    pub rip: u64,
    pub cs: u64,
    pub rflags: u64,
    pub rsp: u64,
    pub ss: u64,
}

const _: () = assert!(core::mem::size_of::<Regs>() == 22 * 8);

/// Selectors user code runs with (gdt.c).
pub const USER_CS: u64 = 0x23;
pub const USER_SS: u64 = 0x1b;

/// The end of the lower half: user addresses are below it.
pub const USER_END: u64 = 0x0000_8000_0000_0000;

pub const EINTR: i64 = 4;
pub const EAGAIN: i64 = 11;
pub const EFAULT: i64 = 14;
pub const EINVAL: i64 = 22;
/// Kernel-internal, never seen by user space: "interrupted by a signal,
/// restart the syscall unless a handler without SA_RESTART runs".
pub const ERESTARTSYS: i64 = 512;
/// Kernel-internal too: -EINTR if a handler runs, else the syscall becomes
/// restart_syscall, which goes on with what the task's SigState kept.
pub const ERESTART_RESTARTBLOCK: i64 = 516;
pub const SYS_RESTART_SYSCALL: u64 = 219;

pub const NS_PER_SEC: u64 = 1_000_000_000;

/// struct kernel_timespec (hubble/syscalls.h).
#[repr(C)]
pub struct Timespec {
    pub tv_sec: i64,
    pub tv_nsec: i64,
}

unsafe extern "C" {
    pub fn printk(fmt: *const c_char, ...);
    pub fn hpet_get_time_ns() -> u64;
    /// The calling task's signal state (kernel/signal.c).
    pub fn signal_current() -> *mut c_void;
    /// Sleep until `cond(arg)` holds (0) or `deadline_ns` passes
    /// (-EAGAIN; 0 = no deadline). Woken by every signal sent to us.
    pub fn signal_wait(
        cond: extern "C" fn(*mut c_void) -> bool,
        arg: *mut c_void,
        deadline_ns: u64,
    ) -> i64;
    /// Whether [addr, addr + len) is user memory mapped in the current
    /// address space.
    pub fn user_mapped(addr: u64, len: u64) -> bool;
}

/// Copy a `T` in from user memory; None if it is not there.
pub fn copy_from_user<T>(addr: u64) -> Option<T> {
    let len = core::mem::size_of::<T>() as u64;
    if addr == 0 || !unsafe { user_mapped(addr, len) } {
        return None;
    }
    // Mapped user memory in our own address space: readable as is.
    Some(unsafe { core::ptr::read_unaligned(addr as *const T) })
}

/// Copy `val` out to user memory; false if it can't go there.
pub fn copy_to_user<T>(addr: u64, val: &T) -> bool {
    let len = core::mem::size_of::<T>() as u64;
    if addr == 0 || !unsafe { user_mapped(addr, len) } {
        return false;
    }
    // A copy-on-write page faults and is copied, as for any kernel write.
    unsafe { core::ptr::write_unaligned(addr as *mut T, core::ptr::read(val)) };
    true
}

/// Interrupts off on this CPU until the guard drops (then back to how
/// they were).
pub struct IrqGuard(u64);

impl IrqGuard {
    pub fn new() -> Self {
        let flags: u64;
        unsafe { core::arch::asm!("pushfq", "pop {}", "cli", out(reg) flags) };
        IrqGuard(flags)
    }
}

impl Drop for IrqGuard {
    fn drop(&mut self) {
        const IF: u64 = 1 << 9;
        if self.0 & IF != 0 {
            unsafe { core::arch::asm!("sti") };
        }
    }
}

/// A spinlock that keeps interrupts off while held: signals are sent from
/// interrupt handlers (Ctrl-C), so holding it with them on could deadlock
/// against our own CPU.
pub struct IrqLock(core::sync::atomic::AtomicBool);

pub struct IrqLockGuard<'a> {
    lock: &'a IrqLock,
    _irq: IrqGuard,
}

impl IrqLock {
    pub fn lock(&self) -> IrqLockGuard<'_> {
        use core::sync::atomic::Ordering;
        let irq = IrqGuard::new();
        while self
            .0
            .compare_exchange_weak(false, true, Ordering::Acquire, Ordering::Relaxed)
            .is_err()
        {
            core::hint::spin_loop();
        }
        IrqLockGuard { lock: self, _irq: irq }
    }
}

impl Drop for IrqLockGuard<'_> {
    fn drop(&mut self) {
        self.lock.0.store(false, core::sync::atomic::Ordering::Release);
        // _irq drops after this: interrupts come back once unlocked.
    }
}
