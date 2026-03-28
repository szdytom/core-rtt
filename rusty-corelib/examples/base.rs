#![no_std]
#![no_main]

use core::hint::spin_loop;
use rusty_corelib::{SensorTile, entry, log_bytes, logln, read_sensor};

fn main() -> ! {
	let dev_info = rusty_corelib::dev_info();
	assert!(
		dev_info.id == 0,
		"This example is only meant to be run on the base runtime!"
	);

	let mut data: [SensorTile; 25] = [SensorTile::uninitialized(); 25];
	assert!(
		read_sensor(&mut data) == Ok(25),
		"Failed to read sensor data!"
	);

	let mut buf: [u8; 80] = [0; 80];
	let header = "Sensor Data:\n";
	buf[..header.len()].copy_from_slice(header.as_bytes());
	let mut offset = header.len();
	for (i, tile) in data.iter().enumerate() {
		buf[offset] = if !tile.visible() {
			'?'
		} else if tile.has_unit() {
			'@'
		} else {
			'.'
		} as u8;
		buf[offset + 1] = if i % 5 == 4 { b'\n' } else { b' ' };
		offset += 2;
	}
	log_bytes(&buf[..offset]).unwrap();
	logln!("Test complete").unwrap();

	loop {
		spin_loop();
	}
}

entry!(main);
