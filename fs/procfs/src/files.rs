//! The files under /proc and the text each one produces.
//!
//! Every file is a generator run once when the file is opened; reads
//! then serve that snapshot, so a reader sees consistent content across
//! partial reads (like Linux seq_file).

use alloc::string::String;
use core::arch::x86_64::__cpuid;
use core::fmt::Write;

use crate::kernel::{hpet_get_time_ns, pmm_get_stats, smp_get_cpu_count};

pub struct ProcFile {
    /// NUL-terminated so it can go straight into a readdir entry.
    pub name: &'static core::ffi::CStr,
    pub generate: fn(&mut String),
}

pub static FILES: &[ProcFile] = &[
    ProcFile { name: c"version", generate: version },
    ProcFile { name: c"uptime", generate: uptime },
    ProcFile { name: c"meminfo", generate: meminfo },
    ProcFile { name: c"cpuinfo", generate: cpuinfo },
];

pub fn find(name: &[u8]) -> Option<&'static ProcFile> {
    FILES.iter().find(|f| f.name.to_bytes() == name)
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
fn as_text(bytes: &[u8]) -> &str {
    let end = bytes.iter().position(|&c| c == 0).unwrap_or(bytes.len());
    core::str::from_utf8(&bytes[..end]).unwrap_or("?")
}
