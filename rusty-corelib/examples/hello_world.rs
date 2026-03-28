#![no_std]
#![no_main]

use core::hint::spin_loop;

use rusty_corelib::{entry, logln};

fn run() -> ! {
	let dev_info = rusty_corelib::dev_info();
	let my_runtime = if dev_info.id == 0 { "Base" } else { "Unit" };
	let _ = logln!("Hello world from {} runtime!", my_runtime);

	let info = rusty_corelib::meta();
	if let Ok(game) = info {
		let _ = logln!(
			"[base] map={}x{} base_size={}",
			game.map_width,
			game.map_height,
			game.base_size
		);
	}

	loop {
		spin_loop();
	}
}

entry!(run);
