//! The files under /proc and the text each one produces.
//!
//! Every file is a generator run once when the file is opened; reads
//! then serve that snapshot, so a reader sees consistent content across
//! partial reads (like Linux seq_file). A file with a `write` handler is
//! also writable; each write() call is handed over whole.
//!
//! The per-task directories /proc/<pid> live in pid.rs.

use alloc::string::String;
use core::arch::x86_64::__cpuid;
use core::fmt::Write;

use alloc::vec;

use crate::kernel::{
    hpet_get_time_ns, pmm_get_stats, printk_get_console_level, printk_get_log, printk_log_size,
    printk_set_console_level, smp_get_cpu_count, vfs_get_mounts,
};
use crate::pid;

pub struct ProcFile {
    /// NUL-terminated so it can go straight into a readdir entry.
    pub name: &'static core::ffi::CStr,
    pub generate: fn(&mut String),
    /// Takes the written bytes; false rejects them (EINVAL).
    pub write: Option<fn(&[u8]) -> bool>,
}

pub static FILES: &[ProcFile] = &[
    ProcFile {
        name: c"version",
        generate: version,
        write: None,
    },
    ProcFile {
        name: c"uptime",
        generate: uptime,
        write: None,
    },
    ProcFile {
        name: c"meminfo",
        generate: meminfo,
        write: None,
    },
    ProcFile {
        name: c"cpuinfo",
        generate: cpuinfo,
        write: None,
    },
    ProcFile {
        name: c"kmsg",
        generate: kmsg,
        write: None,
    },
    ProcFile {
        name: c"loglevel",
        generate: loglevel,
        write: Some(set_loglevel),
    },
    ProcFile {
        name: c"mounts",
        generate: mounts,
        write: None,
    },
    ProcFile {
        name: c"tasks",
        generate: tasks,
        write: None,
    },
];

pub fn find(name: &[u8]) -> Option<&'static ProcFile> {
    FILES.iter().find(|f| f.name.to_bytes() == name)
}

/// One line per task: pid ppid state cpu name.
fn tasks(out: &mut String) {
    for t in pid::snapshot() {
        let _ = writeln!(
            out,
            "{} {} {} {} {}",
            t.pid,
            t.ppid,
            pid::state_char(t.state),
            t.cpu,
            as_text(&t.name)
        );
    }
}

fn version(out: &mut String) {
    let _ = writeln!(out, "Hubble kernel (procfs written in Rust)");
}

fn uptime(out: &mut String) {
    let ns = unsafe { hpet_get_time_ns() };
    let centis = ns / 10_000_000;
    // Second field is idle time, which the scheduler does not track yet.
    let _ = writeln!(out, "{}.{:02} 0.00", centis / 100, centis % 100);
}

fn meminfo(out: &mut String) {
    let (mut total, mut used) = (0u64, 0u64);
    unsafe { pmm_get_stats(&mut total, &mut used) };
    let free = total.saturating_sub(used);
    let _ = writeln!(out, "MemTotal:     {:>10} kB", total / 1024);
    let _ = writeln!(out, "MemFree:      {:>10} kB", free / 1024);
    let _ = writeln!(out, "MemAvailable: {:>10} kB", free / 1024);
}

fn cpuinfo(out: &mut String) {
    let vendor = cpu_vendor();
    let brand = cpu_brand();
    let count = unsafe { smp_get_cpu_count() };
    for cpu in 0..count {
        let _ = writeln!(out, "processor\t: {}", cpu);
        let _ = writeln!(out, "vendor_id\t: {}", as_text(&vendor));
        let _ = writeln!(out, "model name\t: {}", as_text(&brand).trim());
        let _ = writeln!(out);
    }
}

fn mounts(out: &mut String) {
    let mut mount = unsafe { vfs_get_mounts() };
    while !mount.is_null() {
        let mountpoint = unsafe { as_text(&(*mount).mountpoint) };
        let _ = writeln!(out, "{} ", mountpoint);
        mount = unsafe { (*mount).next };
    }
}
/// The kernel log, every level, as far back as the log buffer reaches.
/// Unlike Linux's /proc/kmsg, reading it does not consume it.
fn kmsg(out: &mut String) {
    let mut buf = vec![0u8; unsafe { printk_log_size() }];
    let n = unsafe { printk_get_log(buf.as_mut_ptr() as *mut core::ffi::c_char, buf.len()) };
    buf.truncate(n);
    out.push_str(&String::from_utf8_lossy(&buf));
}

/// Console log level: messages with a lower level reach the screen
/// (`echo 8 > /proc/loglevel` shows everything).
fn loglevel(out: &mut String) {
    let _ = writeln!(out, "{}", unsafe { printk_get_console_level() });
}

fn set_loglevel(data: &[u8]) -> bool {
    let text = core::str::from_utf8(data).unwrap_or("");
    match text.trim().parse::<i32>() {
        Ok(level @ 0..=8) => {
            unsafe { printk_set_console_level(level) };
            true
        }
        _ => false,
    }
}

fn cpu_vendor() -> [u8; 12] {
    let r = __cpuid(0);
    let mut v = [0u8; 12];
    v[0..4].copy_from_slice(&r.ebx.to_le_bytes());
    v[4..8].copy_from_slice(&r.edx.to_le_bytes());
    v[8..12].copy_from_slice(&r.ecx.to_le_bytes());
    v
}

fn cpu_brand() -> [u8; 48] {
    let mut b = [0u8; 48];
    if __cpuid(0x8000_0000).eax < 0x8000_0004 {
        return b;
    }
    for (i, leaf) in (0x8000_0002u32..=0x8000_0004).enumerate() {
        let r = __cpuid(leaf);
        for (j, reg) in [r.eax, r.ebx, r.ecx, r.edx].into_iter().enumerate() {
            let at = i * 16 + j * 4;
            b[at..at + 4].copy_from_slice(&reg.to_le_bytes());
        }
    }
    b
}

/// Bytes up to the first NUL, as text ('?' if not UTF-8).
pub fn as_text(bytes: &[u8]) -> &str {
    let end = bytes.iter().position(|&c| c == 0).unwrap_or(bytes.len());
    core::str::from_utf8(&bytes[..end]).unwrap_or("?")
}
