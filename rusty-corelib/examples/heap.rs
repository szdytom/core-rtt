#![no_std]
#![no_main]

extern crate alloc;

use alloc::vec;
use alloc::vec::Vec;
use core::hint::spin_loop;

use rusty_corelib::entry;

fn run() -> ! {
	let mut malloc_like = Vec::<i32>::with_capacity(8);
	for i in 0..8 {
		malloc_like.push(i * 3);
	}
	for i in 0..8 {
		assert!(malloc_like[i] == i as i32 * 3);
	}

	let calloc_like = vec![0_i32; 8];
	for value in calloc_like {
		assert!(value == 0);
	}

	let mut realloc_like = Vec::<i32>::with_capacity(4);
	for i in 0..4 {
		realloc_like.push(100 + i);
	}
	realloc_like.resize(12, 0);
	for i in 0..4 {
		assert!(realloc_like[i] == 100 + i as i32);
	}
	for i in 4..12 {
		realloc_like[i] = 200 + i as i32;
	}
	realloc_like.truncate(6);
	for i in 0..4 {
		assert!(realloc_like[i] == 100 + i as i32);
	}

	let mut blocks: Vec<Vec<u8>> = Vec::new();
	for _ in 0..6 {
		let block = vec![0x5A_u8; 1024];
		assert!(block.iter().all(|value| *value == 0x5A));
		blocks.push(block);
	}
	drop(blocks);

	assert!(
		rusty_corelib::logf!("[PASS] heap ecall behavior is correct.")
			== Ok(())
	);
	loop {
		spin_loop();
	}
}

entry!(run);
