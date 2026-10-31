//! The signal frame: what a handler finds on its stack, laid out as on
//! Linux x86_64 (struct rt_sigframe), so musl's handlers and ucontext
//! readers work unchanged. setup() builds it, restore() (rt_sigreturn)
//! reads it back.
//!
//!   rsp ->  pretcode     the restorer: the handler's `ret` goes there
//!           ucontext     registers, FPU state pointer, the old mask
//!           siginfo      the handler's second argument
//!           ...
//!           fxsave area  64-byte aligned, above the frame
//!           (128-byte red zone of the interrupted code)

use crate::kernel::*;
use crate::signal::{SA_RESTORER, SigAction, SigInfo};

/// struct sigcontext (asm/sigcontext.h).
#[repr(C)]
#[derive(Clone, Copy)]
struct SigContext {
    r8: u64,
    r9: u64,
    r10: u64,
    r11: u64,
    r12: u64,
    r13: u64,
    r14: u64,
    r15: u64,
    rdi: u64,
    rsi: u64,
    rbp: u64,
    rbx: u64,
    rdx: u64,
    rax: u64,
    rcx: u64,
    rsp: u64,
    rip: u64,
    eflags: u64,
    cs: u16,
    gs: u16,
    fs: u16,
    ss: u16,
    err: u64,
    trapno: u64,
    oldmask: u64,
    cr2: u64,
    fpstate: u64,
    reserved: [u64; 8],
}

#[repr(C)]
#[derive(Clone, Copy)]
struct StackT {
    sp: u64,
    flags: i32,
    size: u64,
}

#[repr(C)]
#[derive(Clone, Copy)]
struct UContext {
    flags: u64,
    link: u64,
    stack: StackT,
    mcontext: SigContext,
    sigmask: u64,
}

/// siginfo_t: 128 bytes. The union starts at 16; its first fields are
/// the sender's pid and uid, or the faulting address.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct UserSigInfo {
    signo: i32,
    errno: i32,
    code: i32,
    _pad: i32,
    fields: [u64; 14],
}

#[repr(C)]
struct RtSigFrame {
    pretcode: u64,
    uc: UContext,
    info: UserSigInfo,
}

const _: () = {
    assert!(core::mem::size_of::<SigContext>() == 256);
    assert!(core::mem::size_of::<UContext>() == 304);
    assert!(core::mem::size_of::<UserSigInfo>() == 128);
};

/// What fxsave writes: 512 bytes, 16-byte aligned.
#[repr(C, align(16))]
struct FxState([u8; 512]);

const SS_DISABLE: i32 = 2;
const RED_ZONE: u64 = 128;

const SI_USER: i32 = 0;
const SIGCHLD: i32 = 17;

/// RFLAGS bits a handler may change through the frame (Linux FIX_EFLAGS:
/// CF PF AF ZF SF TF DF OF AC RF).
const USER_FLAGS: u64 = 0x1 | 0x4 | 0x10 | 0x40 | 0x80 | 0x100 | 0x400 | 0x800 | 0x4_0000 | 0x1_0000;
const FLAG_TF: u64 = 0x100;
const FLAG_DF: u64 = 0x400;

pub fn siginfo(sig: i32, info: &SigInfo) -> UserSigInfo {
    let mut out = UserSigInfo {
        signo: sig,
        errno: 0,
        code: info.code,
        _pad: 0,
        fields: [0; 14],
    };
    if info.addr != 0 {
        out.fields[0] = info.addr; // si_addr
    } else {
        // si_pid, si_uid (always root), then SIGCHLD's si_status.
        out.fields[0] = info.pid as u32 as u64;
        if sig == SIGCHLD {
            out.fields[1] = info.status as u32 as u64;
        }
    }
    if info.code == 0 && info.pid == 0 && info.addr == 0 {
        out.code = SI_USER;
    }
    out
}

fn fxsave() -> FxState {
    let mut fx = FxState([0; 512]);
    // The user's FPU/SSE registers: the kernel itself never uses them.
    unsafe { core::arch::asm!("fxsave64 [{}]", in(reg) fx.0.as_mut_ptr()) };
    fx
}

/// Point `regs` at the handler for `sig`, with a frame to come back by.
/// False if the user stack has no room for it.
pub fn setup(regs: &mut Regs, sig: i32, act: &SigAction, info: &SigInfo, old_mask: u64) -> bool {
    // Without a restorer the handler has nowhere to return to; musl and
    // glibc always set one.
    if act.flags & SA_RESTORER == 0 || act.handler >= USER_END {
        return false;
    }

    let fx = fxsave();
    let fx_addr = (regs.rsp.wrapping_sub(RED_ZONE + 512)) & !63;
    // So that rsp + 8 is 16-byte aligned at entry, as after a call.
    let frame = ((fx_addr - core::mem::size_of::<RtSigFrame>() as u64) & !15) - 8;
    if frame >= USER_END || frame < 4096 {
        return false;
    }

    let mc = SigContext {
        r8: regs.r8,
        r9: regs.r9,
        r10: regs.r10,
        r11: regs.r11,
        r12: regs.r12,
        r13: regs.r13,
        r14: regs.r14,
        r15: regs.r15,
        rdi: regs.rdi,
        rsi: regs.rsi,
        rbp: regs.rbp,
        rbx: regs.rbx,
        rdx: regs.rdx,
        rax: regs.rax,
        rcx: regs.rcx,
        rsp: regs.rsp,
        rip: regs.rip,
        eflags: regs.rflags,
        cs: USER_CS as u16,
        gs: 0,
        fs: 0,
        ss: USER_SS as u16,
        err: regs.err_code,
        trapno: regs.int_no,
        oldmask: old_mask,
        cr2: info.addr,
        fpstate: fx_addr,
        reserved: [0; 8],
    };
    let out = RtSigFrame {
        pretcode: act.restorer,
        uc: UContext {
            flags: 0,
            link: 0,
            stack: StackT {
                sp: 0,
                flags: SS_DISABLE,
                size: 0,
            },
            mcontext: mc,
            sigmask: old_mask,
        },
        info: siginfo(sig, info),
    };

    if !copy_to_user(fx_addr, &fx) || !copy_to_user(frame, &out) {
        return false;
    }

    let info_addr = frame + core::mem::offset_of!(RtSigFrame, info) as u64;
    let uc_addr = frame + core::mem::offset_of!(RtSigFrame, uc) as u64;
    regs.rdi = sig as u64;
    regs.rsi = info_addr;
    regs.rdx = uc_addr;
    regs.rax = 0;
    regs.rip = act.handler;
    regs.rsp = frame;
    regs.rflags &= !(FLAG_TF | FLAG_DF);
    regs.cs = USER_CS;
    regs.ss = USER_SS;
    true
}

/// Load the registers saved by setup() back into `regs`; the mask to
/// restore, or None if the frame is unusable. The handler's `ret` popped
/// pretcode, so the frame starts 8 below the current stack pointer.
pub fn restore(regs: &mut Regs) -> Option<u64> {
    let frame = regs.rsp.wrapping_sub(8);
    let uc: UContext = copy_from_user(frame + core::mem::offset_of!(RtSigFrame, uc) as u64)?;
    let mc = uc.mcontext;
    if mc.rip >= USER_END || mc.rsp >= USER_END {
        return None;
    }

    if mc.fpstate != 0 {
        let mut fx: FxState = copy_from_user(mc.fpstate)?;
        // MXCSR bits the CPU doesn't support would #GP in fxrstor.
        let cur = fxsave();
        let mut mask = u32::from_le_bytes(cur.0[28..32].try_into().unwrap());
        if mask == 0 {
            mask = 0xffbf;
        }
        let mxcsr = u32::from_le_bytes(fx.0[24..28].try_into().unwrap()) & mask;
        fx.0[24..28].copy_from_slice(&mxcsr.to_le_bytes());
        unsafe { core::arch::asm!("fxrstor64 [{}]", in(reg) fx.0.as_ptr()) };
    }

    regs.r8 = mc.r8;
    regs.r9 = mc.r9;
    regs.r10 = mc.r10;
    regs.r11 = mc.r11;
    regs.r12 = mc.r12;
    regs.r13 = mc.r13;
    regs.r14 = mc.r14;
    regs.r15 = mc.r15;
    regs.rdi = mc.rdi;
    regs.rsi = mc.rsi;
    regs.rbp = mc.rbp;
    regs.rbx = mc.rbx;
    regs.rdx = mc.rdx;
    regs.rax = mc.rax;
    regs.rcx = mc.rcx;
    regs.rsp = mc.rsp;
    regs.rip = mc.rip;
    regs.rflags = (regs.rflags & !USER_FLAGS) | (mc.eflags & USER_FLAGS);
    regs.cs = USER_CS;
    regs.ss = USER_SS;
    // Whatever rax now says, this is not an interrupted syscall to redo.
    regs.int_no = u64::MAX;
    Some(uc.sigmask)
}
