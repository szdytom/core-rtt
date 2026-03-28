#![no_std]
#![no_main]

use rusty_corelib::entry;

fn run() -> ! {
	rusty_corelib::abort()
}

entry!(run);
