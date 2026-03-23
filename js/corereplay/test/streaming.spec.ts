import { describe, expect, test } from 'vitest';
import { decodeReplayFromStream } from '../src/decode.js';
import { decodeReplayStream } from '../src/stream.js';
import { sampleReplayBytes } from './helpers.js';

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
});
