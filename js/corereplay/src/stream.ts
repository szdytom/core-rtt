import type {
	DecodeOptions,
	ReplayStreamEvent,
	ReplayStreamInput,
} from './api.js';
import type { ReplayInput } from './types.js';
import { ReplayDecoderCore } from './decoder_core.js';

export async function* decodeReplayStream(
	input: ReplayStreamInput,
	options: DecodeOptions = {},
): AsyncGenerator<ReplayStreamEvent> {
	const decoder = new ReplayDecoderCore(options);

	for await (const chunk of toAsyncIterable(input)) {
		decoder.push(chunk);

		while (true) {
			const result = decoder.read();
			if (result.kind === 'need-more-data') {
				break;
			}

			if (result.kind === 'end') {
				return { kind: 'end', endMarker: result.endMarker };
			}

			if (result.kind === 'header') {
				yield { kind: 'header', header: result.header };
			} else if (result.kind === 'tick') {
				yield { kind: 'tick', tick: result.tick };
			}
		}
	}

	decoder.finalize();
	// finalize() ensures end marker exists.
	return { kind: 'end', endMarker: decoder.state().endMarker! };
}

async function* toAsyncIterable(
	input: ReplayStreamInput,
): AsyncGenerator<ReplayInput, void, void> {
	if (typeof (input as AsyncIterable<ReplayInput>)[Symbol.asyncIterator] === 'function') {
		for await (const chunk of input as AsyncIterable<ReplayInput>) {
			yield chunk;
		}
		return;
	}

	const reader = (input as ReadableStream<Uint8Array>).getReader();
	try {
		while (true) {
			const next = await reader.read();
			if (next.done) {
				break;
			}
			yield next.value;
		}
	} finally {
		reader.releaseLock();
	}
}
