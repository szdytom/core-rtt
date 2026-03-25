import { describe, expect, test } from 'vitest';
import { decodeReplay } from '../src/decode.js';
import { ReplayDecodeError } from '../src/errors.js';
import { concatBytes, leU16, sampleReplayBytes, u8 } from './helpers.js';

describe('decodeReplay', () => {
	test('decodes a valid replay', () => {
		const replay = decodeReplay(sampleReplayBytes());

		expect(replay.header.magic).toBe('CRPL');
		expect(replay.header.version).toBe(4);
		expect(replay.header.tilemap.width).toBe(2);
		expect(replay.ticks).toHaveLength(1);
		expect(replay.ticks[0].tick).toBe(5);
		expect(replay.ticks[0].units).toHaveLength(1);
		expect(replay.ticks[0].logs[0].payload).toEqual(u8(0x41, 0x42, 0x43));
		expect(replay.endMarker.termination).toBe('completed');
		expect(replay.endMarker.winnerPlayerId).toBe(1);
	});

	test('throws on bad magic', () => {
		const bytes = sampleReplayBytes();
		bytes[0] = 0x58;

		expect(() => decodeReplay(bytes)).toThrowError(ReplayDecodeError);
		try {
			decodeReplay(bytes);
		} catch (error) {
			const decode_error = error as ReplayDecodeError;
			expect(decode_error.code).toBe('BAD_MAGIC');
		}
	});

	test('throws on unsupported version', () => {
		const bytes = sampleReplayBytes();
		bytes[4] = 3;

		expect(() => decodeReplay(bytes)).toThrowError(ReplayDecodeError);
		try {
			decodeReplay(bytes);
		} catch (error) {
			const decode_error = error as ReplayDecodeError;
			expect(decode_error.code).toBe('UNSUPPORTED_VERSION');
		}
	});

	test('throws on truncated replay', () => {
		const bytes = sampleReplayBytes().subarray(0, 12);

		expect(() => decodeReplay(bytes)).toThrowError(ReplayDecodeError);
		try {
			decodeReplay(bytes);
		} catch (error) {
			const decode_error = error as ReplayDecodeError;
			expect(decode_error.code).toBe('MISSING_HEADER');
		}
	});

	test('decodes header-only replay in non-strict mode', () => {
		const full = sampleReplayBytes();
		const header_size = full[6] | (full[7] << 8);
		const header_total_size = 8 + header_size;
		const header_only = full.subarray(0, header_total_size);

		const replay = decodeReplay(header_only, { strict: false });
		expect(replay.ticks).toHaveLength(0);
		expect(replay.endMarker.termination).toBe('aborted');
		expect(replay.endMarker.winnerPlayerId).toBe(0);
	});

	test('decodes replay without end marker in non-strict mode', () => {
		const full = sampleReplayBytes();
		const missing_end_marker = full.subarray(0, full.length - 3);

		expect(() => decodeReplay(missing_end_marker)).toThrowError(ReplayDecodeError);

		const replay = decodeReplay(missing_end_marker, { strict: false });
		expect(replay.ticks).toHaveLength(1);
		expect(replay.endMarker.termination).toBe('aborted');
		expect(replay.endMarker.winnerPlayerId).toBe(0);
	});

	test('ignores truncated trailing tick in non-strict mode', () => {
		const full = sampleReplayBytes();
		const missing_end_marker = full.subarray(0, full.length - 3);
		const partial_second_tick = u8(
			1,
			6, 0, 0, 0,
			10, 0, 0, 0,
			0xaa, 0xbb,
		);
		const truncated_tail = concatBytes(missing_end_marker, partial_second_tick);

		expect(() => decodeReplay(truncated_tail)).toThrowError(ReplayDecodeError);

		const replay = decodeReplay(truncated_tail, { strict: false });
		expect(replay.ticks).toHaveLength(1);
		expect(replay.ticks[0].tick).toBe(5);
		expect(replay.endMarker.termination).toBe('aborted');
		expect(replay.endMarker.winnerPlayerId).toBe(0);
	});

	test('throws when unit count exceeds maximum', () => {
		const bytes = sampleReplayBytes();

		// Locate unit_count after: header(18) + tick_type/tick/size(9) + players(8)
		const unit_count_offset = 18 + 9 + 8;
		const mutated = concatBytes(bytes.subarray(0, unit_count_offset), leU16(31), bytes.subarray(unit_count_offset + 2));

		expect(() => decodeReplay(mutated)).toThrowError(ReplayDecodeError);
		try {
			decodeReplay(mutated);
		} catch (error) {
			const decode_error = error as ReplayDecodeError;
			expect(decode_error.code).toBe('UNIT_COUNT_EXCEEDS_MAXIMUM');
		}
	});

	test('throws on invalid end marker payload', () => {
		const bytes = sampleReplayBytes();
		bytes[bytes.length - 1] = 0;

		expect(() => decodeReplay(bytes)).toThrowError(ReplayDecodeError);
		try {
			decodeReplay(bytes);
		} catch (error) {
			const decode_error = error as ReplayDecodeError;
			expect(decode_error.code).toBe('INVALID_END_MARKER_PAYLOAD');
		}
	});

	test('ignores extra bytes at end of header payload', () => {
		const base = sampleReplayBytes();

		const old_header_size = 10;
		const new_header_size = 12;
		const header_prefix = base.subarray(0, 6);
		const tilemap_payload = base.subarray(8, 8 + old_header_size);
		const tail_records = base.subarray(8 + old_header_size);

		const patched = concatBytes(
			header_prefix,
			leU16(new_header_size),
			tilemap_payload,
			u8(0xaa, 0xbb),
			tail_records,
		);

		const replay = decodeReplay(patched);
		expect(replay.header.headerSize).toBe(new_header_size);
		expect(replay.ticks).toHaveLength(1);
	});
});
