export function u8(...values: number[]): Uint8Array {
	return Uint8Array.from(values.map((value) => value & 0xff));
}

export function concatBytes(...chunks: Uint8Array[]): Uint8Array {
	const total = chunks.reduce((sum, chunk) => sum + chunk.length, 0);
	const merged = new Uint8Array(total);
	let pos = 0;
	for (const chunk of chunks) {
		merged.set(chunk, pos);
		pos += chunk.length;
	}
	return merged;
}

export function leU16(value: number): Uint8Array {
	return u8(value, value >> 8);
}

export function leI16(value: number): Uint8Array {
	return leU16(value & 0xffff);
}

export function leU32(value: number): Uint8Array {
	return u8(value, value >> 8, value >> 16, value >> 24);
}

export function sampleReplayBytesWithPayload(payload: Uint8Array): Uint8Array {
	const tilemap = concatBytes(
		leU16(2),
		leU16(1),
		leU16(1),
		leU16(0),
		u8(0b00010001, 0b00100110),
	);

	const game_rules = concatBytes(
		leU16(2),
		leU16(1),
		leU16(1),
		u8(100),
		leU32(1),
		leU16(25),
		u8(3),
		leU16(200),
		leU16(1000),
		u8(5),
		u8(9),
		leU16(400),
		leU16(1000),
		leU16(600),
		leU16(500),
		leU16(8),
	);

	const header = concatBytes(
		u8(0x43, 0x52, 0x50, 0x4c),
		leU16(5),
		leU16(tilemap.length + game_rules.length),
		tilemap,
		game_rules,
	);

	const log_entry = concatBytes(
		leU32(5),
		u8(1),
		u8(7),
		u8(1),
		u8(0),
		leU16(payload.length),
		payload,
	);

	const tick_payload = concatBytes(
		u8(1), leU16(100), u8(0),
		u8(2), leU16(110), u8(3),
		leU16(1),
		u8(7, 1), leI16(10), leI16(-2), u8(40), leU16(90), u8(4), u8(2),
		leU16(1),
		leI16(2), leI16(3), u8(1), u8(1), u8(12),
		leU16(1),
		log_entry,
	);

	const tick = concatBytes(
		u8(1),
		leU32(5),
		leU32(tick_payload.length),
		tick_payload,
	);

	const end = u8(255, 0, 1);
	return concatBytes(header, tick, end);
}
