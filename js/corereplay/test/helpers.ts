export function u8(...values: number[]): Uint8Array {
	return Uint8Array.from(values.map((v) => v & 0xff));
}

export function concatBytes(...chunks: Uint8Array[]): Uint8Array {
	const total = chunks.reduce((sum, c) => sum + c.length, 0);
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
	const normalized = value & 0xffff;
	return leU16(normalized);
}

export function leU32(value: number): Uint8Array {
	return u8(value, value >> 8, value >> 16, value >> 24);
}

export function sampleReplayBytes(): Uint8Array {
	const tilemap = concatBytes(
		leU16(2),
		leU16(1),
		leU16(1),
		leU16(0),
		u8(0b00010001, 0b00100110),
	);

	const header = concatBytes(
		u8(0x43, 0x52, 0x50, 0x4c),
		leU16(4),
		leU16(tilemap.length),
		tilemap,
	);

	const tick_payload = concatBytes(
		u8(1), leU16(100), u8(0),
		u8(2), leU16(110), u8(3),
		leU16(1),
		u8(7, 1), leI16(10), leI16(-2), u8(40), leU16(90), u8(4), u8(2),
		leU16(1),
		leI16(2), leI16(3), u8(1), u8(1), u8(12),
		leU16(1),
		leU32(5), u8(1), u8(7), u8(1), u8(0), leU16(3), u8(0x41, 0x42, 0x43),
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
