#![no_std]
#![no_main]

use core::ffi::c_char;
use core::fmt::Write;

mod kernel;
mod sigframe;
mod signal;

/// Formats into a fixed buffer: the panic message can't allocate.
struct Buf {
    data: [u8; 256],
    len: usize,
}

impl Write for Buf {
    fn write_str(&mut self, s: &str) -> core::fmt::Result {
        let room = self.data.len() - 1 - self.len; // keep the NUL
        let n = s.len().min(room);
        self.data[self.len..self.len + n].copy_from_slice(&s.as_bytes()[..n]);
        self.len += n;
        Ok(())
    }
}

#[panic_handler]
fn panic(info: &core::panic::PanicInfo) -> ! {
    let mut buf = Buf { data: [0; 256], len: 0 };
    let _ = write!(buf, "<0>rust panic: {}\n", info);
    unsafe {
        kernel::printk(c"%s".as_ptr(), buf.data.as_ptr());
        core::arch::asm!("cli");
    }
    loop {
        unsafe { core::arch::asm!("hlt") };
    }
}

unsafe extern "C" {
    fn rust_printk(msg: *const c_char);
}

#[unsafe(no_mangle)]
pub extern "C" fn rust_init() {
    unsafe {
        rust_printk(c"Hello from Rust!\n".as_ptr());
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn rust_sum(a: i32, b: i32) -> i32 {
    a + b
}
