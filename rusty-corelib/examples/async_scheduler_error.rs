#![no_std]
#![no_main]

use rusty_corelib::{ErrorCode, Scheduler, entry, logf};

fn run() -> ! {
	let mut scheduler = Scheduler::new();
	scheduler.spawn(async {
		rusty_corelib::send_msg(&[])?;
		Ok(())
	});

	let result = scheduler.run_until_complete();
	match result {
		Err(ErrorCode::OutOfRange) => {
			let _ =
				logf!("[PASS] async scheduler error propagation is correct.");
		}
		Err(other) => {
			let _ = logf!(
				"[FAIL] async scheduler got unexpected error: {:?}",
				other
			);
		}
		Ok(()) => {
			let _ = logf!("[FAIL] async scheduler unexpectedly succeeded.");
		}
	}

	loop {
		core::hint::spin_loop();
	}
}

entry!(run);
