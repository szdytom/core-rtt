import { describe, expect, test } from 'vitest';
import { decodeReplayFromStream } from '../src/decode.js';
import { decodeReplayStream } from '../src/stream.js';
import { concatBytes, sampleReplayBytes, u8 } from './helpers.js';

async function* chunkedAsyncIterable(
	bytes: Uint8Array,
	chunk_size: number,
): AsyncGenerator<Uint8Array, void, void> {
	for (let i = 0; i < bytes.length; i += chunk_size) {
		yield bytes.subarray(i, Math.min(bytes.length, i + chunk_size));
	}
}

function chunkedReadableStream(bytes: Uint8Array, chunk_size: number): ReadableStream<Uint8Array> {
	return new ReadableStream<Uint8Array>({
		start(controller) {
			for (let i = 0; i < bytes.length; i += chunk_size) {
				controller.enqueue(bytes.subarray(i, Math.min(bytes.length, i + chunk_size)));
			}
			controller.close();
		},
	});
}

describe('decodeReplayStream', () => {
	test('decodes async iterable stream', async () => {
		const bytes = sampleReplayBytes();
		const events: string[] = [];

		for await (const event of decodeReplayStream(chunkedAsyncIterable(bytes, 3))) {
			events.push(event.kind);
		}

		expect(events).toEqual(['header', 'tick', 'end']);
	});

	test('decodes readable stream', async () => {
		const bytes = sampleReplayBytes();
		const replay = await decodeReplayFromStream(chunkedReadableStream(bytes, 4));

		expect(replay.header.magic).toBe('CRPL');
		expect(replay.ticks).toHaveLength(1);
		expect(replay.endMarker.termination).toBe('completed');
	});

	test('works with single-byte chunks', async () => {
		const bytes = sampleReplayBytes();
		const replay = await decodeReplayFromStream(chunkedAsyncIterable(bytes, 1));

		expect(replay.ticks[0].tick).toBe(5);
	});

	test('decodes rule-draw end marker from stream', async () => {
		const bytes = sampleReplayBytes();
		bytes[bytes.length - 2] = 2;
		bytes[bytes.length - 1] = 0;

		const replay = await decodeReplayFromStream(chunkedReadableStream(bytes, 3));

		expect(replay.endMarker.termination).toBe('rule-draw');
		expect(replay.endMarker.winnerPlayerId).toBe(0);
	});

	test('synthesizes aborted end marker for header-only stream in non-strict mode', async () => {
		const full = sampleReplayBytes();
		const header_size = full[6] | (full[7] << 8);
		const header_only = full.subarray(0, 8 + header_size);

		const replay = await decodeReplayFromStream(
			chunkedAsyncIterable(header_only, 2),
			{ strict: false },
		);

		expect(replay.ticks).toHaveLength(0);
		expect(replay.endMarker.termination).toBe('aborted');
		expect(replay.endMarker.winnerPlayerId).toBe(0);
	});

	test('synthesizes aborted end marker when stream is missing end marker', async () => {
		const full = sampleReplayBytes();
		const missing_end_marker = full.subarray(0, full.length - 3);

		const events: string[] = [];
		for await (const event of decodeReplayStream(
			chunkedAsyncIterable(missing_end_marker, 3),
			{ strict: false },
		)) {
			events.push(event.kind);
			if (event.kind === 'end') {
				expect(event.endMarker.termination).toBe('aborted');
				expect(event.endMarker.winnerPlayerId).toBe(0);
			}
		}

		expect(events).toEqual(['header', 'tick', 'end']);
	});

	test('ignores truncated trailing tick in stream non-strict mode', async () => {
		const full = sampleReplayBytes();
		const missing_end_marker = full.subarray(0, full.length - 3);
		const partial_second_tick = u8(
			1,
			6, 0, 0, 0,
			10, 0, 0, 0,
			0xaa, 0xbb,
		);
		const truncated_tail = concatBytes(missing_end_marker, partial_second_tick);

		const replay = await decodeReplayFromStream(
			chunkedAsyncIterable(truncated_tail, 2),
			{ strict: false },
		);

		expect(replay.ticks).toHaveLength(1);
		expect(replay.ticks[0].tick).toBe(5);
		expect(replay.endMarker.termination).toBe('aborted');
		expect(replay.endMarker.winnerPlayerId).toBe(0);
	});
});
