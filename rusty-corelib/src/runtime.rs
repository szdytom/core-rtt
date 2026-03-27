#[cfg(all(feature = "runtime", target_arch = "riscv32"))]
use core::arch::asm;

#[cfg(all(feature = "runtime", target_arch = "riscv32"))]
unsafe fn init_gp() {
    // SAFETY: This is the canonical sequence used to initialize gp.
    unsafe {
        asm!(
            ".option push",
            ".option norelax",
            "1: auipc gp, %pcrel_hi(__global_pointer$)",
            "addi gp, gp, %pcrel_lo(1b)",
            ".option pop",
            options(nostack)
        );
    }
}

#[cfg(all(feature = "runtime", target_arch = "riscv32"))]
unsafe fn clear_bss() {
    unsafe extern "C" {
        static mut __bss_start: u8;
        static mut __BSS_END__: u8;
    }

    let mut p = core::ptr::addr_of_mut!(__bss_start);
    let end = core::ptr::addr_of_mut!(__BSS_END__);

    while p < end {
        // SAFETY: p iterates over the writable .bss interval.
        unsafe { p.write_volatile(0) };
        p = p.wrapping_add(1);
    }
}

#[cfg(all(feature = "runtime", target_arch = "riscv32"))]
unsafe fn run_init_array() {
    unsafe extern "C" {
        static __init_array_start: usize;
        static __init_array_end: usize;
    }

    let start = core::ptr::addr_of!(__init_array_start) as *const extern "C" fn();
    let end = core::ptr::addr_of!(__init_array_end) as *const extern "C" fn();

    let mut p = start;
    while p < end {
        // SAFETY: Function pointers come from linker-provided init array.
        unsafe { (*p)() };
        p = p.wrapping_add(1);
    }
}

#[cfg(all(feature = "runtime", target_arch = "riscv32"))]
unsafe fn call_app_main() -> ! {
    unsafe extern "Rust" {
        fn app_main() -> !;
    }

    // SAFETY: app_main is provided by user crate via `entry!` macro.
    unsafe { app_main() }
}

#[cfg(all(feature = "runtime", target_arch = "riscv32"))]
#[unsafe(no_mangle)]
pub extern "C" fn _start() -> ! {
    // SAFETY: Required startup sequence for freestanding runtime.
    unsafe { init_gp() };

    core::sync::atomic::compiler_fence(core::sync::atomic::Ordering::SeqCst);

    // SAFETY: Required startup sequence for freestanding runtime.
    unsafe { clear_bss() };

    // SAFETY: Required startup sequence for freestanding runtime.
    unsafe { run_init_array() };

    // SAFETY: Transfers control to user entry and must not return.
    unsafe { call_app_main() }
}

#[cfg(all(feature = "runtime", target_os = "none"))]
#[panic_handler]
fn panic(_info: &core::panic::PanicInfo<'_>) -> ! {
    crate::abort()
}
