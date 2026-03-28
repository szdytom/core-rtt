#![no_std]
#![no_main]

use core::hint::spin_loop;

use rusty_corelib::{
	Direction, Result, Scheduler, async_rt::wait_next_turn, entry,
};

async fn move_along_path(path: &[Direction]) -> Result<()> {
	for &direction in path {
		rusty_corelib::async_rt::move_unit(direction).await?;
	}
	Ok(())
}

fn run() -> ! {
	let dev_info = rusty_corelib::dev_info();
	assert!(dev_info.id != 0, "expected to run on unit instead of base");

	if dev_info.id != 1 {
		// Only run the example on the first unit to avoid too many log messages
		loop {
			spin_loop();
		}
	}

	const PATH: [Direction; 4] = [
		Direction::Right,
		Direction::Down,
		Direction::Left,
		Direction::Up,
	];

	let mut scheduler = Scheduler::with_capacity(3);

	scheduler.spawn(async {
		wait_next_turn().await;
		rusty_corelib::send_msg(b"PING")
	});

	scheduler.spawn(async {
		let mut buf = [0_u8; 32];
		let n = rusty_corelib::async_rt::recv_msg(&mut buf).await?;
		if &buf[..n] == b"PING" {
			rusty_corelib::logf!("recv_msg async task got PING")?;
		}
		Ok(())
	});

	scheduler.spawn(async {
		move_along_path(&PATH).await?;
		rusty_corelib::logf!("move_along_path finished")?;
		Ok(())
	});

	if let Err(err) = scheduler.run_until_complete() {
		let _ = rusty_corelib::logf!("async scheduler failed: {:?}", err);
		rusty_corelib::abort();
	}

	let _ = rusty_corelib::logf!("async tasks completed");
	loop {
		spin_loop();
	}
}

entry!(run);
