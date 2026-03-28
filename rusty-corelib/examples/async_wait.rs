#![no_std]
#![no_main]

use core::hint::spin_loop;

use rusty_corelib::{entry, logf};

fn run() -> ! {
	let info = rusty_corelib::dev_info();
	if info.id != 1 {
		loop {
			spin_loop();
		}
	}

	let t0 = rusty_corelib::turn();
	rusty_corelib::block_on(async {
		rusty_corelib::async_rt::wait_next_turn().await;
	});
	let t1 = rusty_corelib::turn();
	assert!(t1 > t0);

	rusty_corelib::block_on(async {
		rusty_corelib::async_rt::wait_for(2).await;
	});
	let t2 = rusty_corelib::turn();
	assert!(t2 >= t1 + 2);

	let target = rusty_corelib::turn() + 1;
	rusty_corelib::block_on(async {
		rusty_corelib::async_rt::wait_until(target).await;
	});
	assert!(rusty_corelib::turn() >= target);

	assert!(logf!("[PASS] async wait primitives are correct.") == Ok(()));
	loop {
		spin_loop();
	}
}

entry!(run);
