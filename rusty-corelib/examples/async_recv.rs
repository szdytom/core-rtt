#![no_std]
#![no_main]

use core::hint::spin_loop;

use rusty_corelib::{Result, Scheduler, entry};

async fn sender_task() -> Result<()> {
	rusty_corelib::async_rt::wait_next_turn().await;
	rusty_corelib::send_msg(b"PING")
}

async fn receiver_task() -> Result<()> {
	let mut buf = [0_u8; 32];
	let n = rusty_corelib::async_rt::recv_msg(&mut buf).await?;
	assert!(n == 4);
	assert!(&buf[..n] == b"PING");
	rusty_corelib::logf!("[PASS] async recv_msg behavior is correct.")?;
	Ok(())
}

fn run() -> ! {
	let info = rusty_corelib::dev_info();
	if info.id != 1 {
		loop {
			spin_loop();
		}
	}

	let mut scheduler = Scheduler::with_capacity(2);
	scheduler.spawn(sender_task());
	scheduler.spawn(receiver_task());
	assert!(scheduler.run_until_complete() == Ok(()));

	loop {
		spin_loop();
	}
}

entry!(run);
