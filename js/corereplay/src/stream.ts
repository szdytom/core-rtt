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
): AsyncGenerator<ReplayStreamEvent, import('./types.js').ReplayEndMarker, void> {
	const decoder = new ReplayDecoderCore(options);
	let end_emitted = false;

	for await (const chunk of toAsyncIterable(input)) {
		decoder.push(chunk);

		while (true) {
			const result = decoder.read();
			if (result.kind === 'need-more-data') {
				break;
			}
			if (result.kind === 'header') {
				yield { kind: 'header', header: result.header };
				continue;
			}
			if (result.kind === 'tick') {
				yield { kind: 'tick', tick: result.tick };
				continue;
			}
			end_emitted = true;
			yield { kind: 'end', endMarker: result.endMarker };
			return result.endMarker;
		}
	}

	decoder.finalize();
	const state = decoder.state();
	if (!end_emitted) {
		yield { kind: 'end', endMarker: state.endMarker! };
	}
	// finalize() ensures end marker exists.
	return state.endMarker!;
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
