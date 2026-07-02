#![no_std]
#![no_main]

use core::ffi::c_char;

#[panic_handler]
fn panic(_: &core::panic::PanicInfo) -> ! {
    loop {}
}

unsafe extern "C" {
    fn rust_printk(msg: *const c_char);
    fn printk(msg: *const c_char);
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
