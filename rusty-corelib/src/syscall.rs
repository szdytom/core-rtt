#[cfg(target_arch = "riscv32")]
use core::arch::asm;

pub const EC_ABORT: u32 = 0x00;
pub const EC_LOG: u32 = 0x01;
pub const EC_META: u32 = 0x0F;
pub const EC_TURN: u32 = 0x10;
pub const EC_DEV_INFO: u32 = 0x11;
pub const EC_READ_SENSOR: u32 = 0x12;
pub const EC_RECV_MSG: u32 = 0x13;
pub const EC_SEND_MSG: u32 = 0x14;
pub const EC_POS: u32 = 0x15;
pub const EC_MANUFACT: u32 = 0x20;
pub const EC_REPAIR: u32 = 0x21;
pub const EC_UPGRADE: u32 = 0x22;
pub const EC_FIRE: u32 = 0x30;
pub const EC_MOVE: u32 = 0x31;
pub const EC_DEPOSIT: u32 = 0x32;
pub const EC_WITHDRAW: u32 = 0x33;
pub const EC_MALLOC: u32 = 0x40;
pub const EC_CALLOC: u32 = 0x41;
pub const EC_REALLOC: u32 = 0x42;
pub const EC_FREE: u32 = 0x43;
pub const EC_MEMINFO: u32 = 0x44;
pub const EC_RAND: u32 = 0x4F;

#[cfg(target_arch = "riscv32")]
unsafe fn call0(call_id: u32) -> i32 {
	let out: i32;
	// SAFETY: Uses the documented ecall ABI for core RTT runtime.
	unsafe {
		asm!(
			"ecall",
			in("a7") call_id,
			lateout("a0") out,
			options(nostack)
		);
	}
	out
}

#[cfg(target_arch = "riscv32")]
unsafe fn call1(call_id: u32, arg0: i32) -> i32 {
	let mut a0 = arg0;
	// SAFETY: Uses the documented ecall ABI for core RTT runtime.
	unsafe {
		asm!(
			"ecall",
			in("a7") call_id,
			inlateout("a0") a0,
			options(nostack)
		);
	}
	a0
}

#[cfg(target_arch = "riscv32")]
unsafe fn call2(call_id: u32, arg0: i32, arg1: i32) -> i32 {
	let mut a0 = arg0;
	// SAFETY: Uses the documented ecall ABI for core RTT runtime.
	unsafe {
		asm!(
			"ecall",
			in("a7") call_id,
			inlateout("a0") a0,
			in("a1") arg1,
			options(nostack)
		);
	}
	a0
}

#[cfg(not(target_arch = "riscv32"))]
unsafe fn call0(_call_id: u32) -> i32 {
	panic!("ecall requires riscv32 target");
}

#[cfg(not(target_arch = "riscv32"))]
unsafe fn call1(_call_id: u32, _arg0: i32) -> i32 {
	panic!("ecall requires riscv32 target");
}

#[cfg(not(target_arch = "riscv32"))]
unsafe fn call2(_call_id: u32, _arg0: i32, _arg1: i32) -> i32 {
	panic!("ecall requires riscv32 target");
}

#[inline(always)]
pub unsafe fn abort() -> ! {
	// SAFETY: Runtime guarantees ecall 0x00 aborts and never returns.
	let _ = unsafe { call0(EC_ABORT) };
	unreachable!()
}

#[inline(always)]
pub unsafe fn log(ptr: *const u8, len: usize) -> i32 {
	// SAFETY: Follows runtime ABI, pointer validity checked by runtime.
	unsafe { call2(EC_LOG, ptr as i32, len as i32) }
}

#[inline(always)]
pub unsafe fn meta(out: *mut u8) -> i32 {
	// SAFETY: Follows runtime ABI, output pointer validity checked by runtime.
	unsafe { call1(EC_META, out as i32) }
}

#[inline(always)]
pub unsafe fn turn() -> i32 {
	// SAFETY: Direct runtime ecall with no arguments.
	unsafe { call0(EC_TURN) }
}

#[inline(always)]
pub unsafe fn dev_info_raw() -> u32 {
	// SAFETY: Raw return payload decoded by higher-level type helpers.
	unsafe { call0(EC_DEV_INFO) as u32 }
}

#[inline(always)]
pub unsafe fn read_sensor(out: *mut u8) -> i32 {
	// SAFETY: Follows runtime ABI, output pointer validity checked by runtime.
	unsafe { call1(EC_READ_SENSOR, out as i32) }
}

#[inline(always)]
pub unsafe fn recv_msg(out: *mut u8, max_len: usize) -> i32 {
	// SAFETY: Follows runtime ABI, pointer validity checked by runtime.
	unsafe { call2(EC_RECV_MSG, out as i32, max_len as i32) }
}

#[inline(always)]
pub unsafe fn send_msg(data: *const u8, len: usize) -> i32 {
	// SAFETY: Follows runtime ABI, pointer validity checked by runtime.
	unsafe { call2(EC_SEND_MSG, data as i32, len as i32) }
}

#[inline(always)]
pub unsafe fn pos_raw() -> u32 {
	// SAFETY: Raw return payload decoded by higher-level type helpers.
	unsafe { call0(EC_POS) as u32 }
}

#[inline(always)]
pub unsafe fn manufact(id: i32) -> i32 {
	// SAFETY: Runtime validates id and caller runtime type.
	unsafe { call1(EC_MANUFACT, id) }
}

#[inline(always)]
pub unsafe fn repair(id: i32) -> i32 {
	// SAFETY: Runtime validates id and caller runtime type.
	unsafe { call1(EC_REPAIR, id) }
}

#[inline(always)]
pub unsafe fn upgrade(id: i32, kind: i32) -> i32 {
	// SAFETY: Runtime validates parameters and caller runtime type.
	unsafe { call2(EC_UPGRADE, id, kind) }
}

#[inline(always)]
pub unsafe fn fire(direction: i32, power: i32) -> i32 {
	// SAFETY: Runtime validates direction and energy constraints.
	unsafe { call2(EC_FIRE, direction, power) }
}

#[inline(always)]
pub unsafe fn move_unit(direction: i32) -> i32 {
	// SAFETY: Runtime validates direction and caller runtime type.
	unsafe { call1(EC_MOVE, direction) }
}

#[inline(always)]
pub unsafe fn deposit(amount: i32) -> i32 {
	// SAFETY: Runtime validates amount and caller runtime type.
	unsafe { call1(EC_DEPOSIT, amount) }
}

#[inline(always)]
pub unsafe fn withdraw(amount: i32) -> i32 {
	// SAFETY: Runtime validates amount and caller runtime type.
	unsafe { call1(EC_WITHDRAW, amount) }
}

#[inline(always)]
pub unsafe fn malloc(size: usize) -> *mut u8 {
	// SAFETY: Runtime allocator validates size and returns managed pointer/null.
	unsafe { call1(EC_MALLOC, size as i32) as *mut u8 }
}

#[inline(always)]
pub unsafe fn calloc(nmemb: usize, size: usize) -> *mut u8 {
	// SAFETY: Runtime allocator validates args and returns managed pointer/null.
	unsafe { call2(EC_CALLOC, nmemb as i32, size as i32) as *mut u8 }
}

#[inline(always)]
pub unsafe fn realloc(ptr: *mut u8, size: usize) -> *mut u8 {
	// SAFETY: Runtime allocator validates args and returns managed pointer/null.
	unsafe { call2(EC_REALLOC, ptr as i32, size as i32) as *mut u8 }
}

#[inline(always)]
pub unsafe fn free(ptr: *mut u8) {
	// SAFETY: Runtime allocator accepts null and owned pointers.
	let _ = unsafe { call1(EC_FREE, ptr as i32) };
}

#[inline(always)]
pub unsafe fn meminfo(out: *mut u8) -> i32 {
	// SAFETY: Follows runtime ABI, output pointer validity checked by runtime.
	unsafe { call1(EC_MEMINFO, out as i32) }
}

#[inline(always)]
pub unsafe fn rand_u32() -> u32 {
	// SAFETY: Direct runtime ecall with no arguments.
	unsafe { call0(EC_RAND) as u32 }
}
