#![no_std]
#![no_main]

use core::hint::spin_loop;

use rusty_corelib::{entry, logln};

fn run() -> ! {
    let _ = logln!("first message");
    let _ = logln!("second message");
    let _ = logln!("third message");
    let _ = logln!("[PASS] rust log simple test");

    loop {
        spin_loop();
    }
}

entry!(run);
