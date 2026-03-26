import type { DecodeOptions, ReplayStreamInput } from './api.js';
import { ReplayDecoderCore } from './decoder_core.js';
import { ReplayDecodeError } from './errors.js';
import { decodeReplayStream } from './stream.js';
import type { ReplayData, ReplayInput } from './types.js';
import assert from 'assert';

export function decodeReplay(
	input: ReplayInput,
	options: DecodeOptions = {},
): ReplayData {
	const decoder = new ReplayDecoderCore(options);
	decoder.push(input);

	let header: ReplayData['header'] | undefined;
	const ticks: ReplayData['ticks'] = [];

	while (true) {
		const result = decoder.read();
		if (result.kind === 'need-more-data') {
			break;
		}
		if (result.kind === 'header') {
			header = result.header;
			continue;
		}
		if (result.kind === 'tick') {
			ticks.push(result.tick);
			continue;
		}
		break;
	}

	const end_marker = decoder.finalize();
	assert(header !== undefined, 'Header should have been parsed if finalize() succeeded');
	return {
		header,
		ticks,
		endMarker: end_marker,
	};
}

export async function decodeReplayFromStream(
	input: ReplayStreamInput,
	options: DecodeOptions = {},
): Promise<ReplayData> {
	let header: ReplayData['header'] | undefined;
	let end_marker: ReplayData['endMarker'] | undefined;
	const ticks: ReplayData['ticks'] = [];

	for await (const event of decodeReplayStream(input, options)) {
		if (event.kind === 'header') {
			header = event.header;
		} else if (event.kind === 'tick') {
			ticks.push(event.tick);
		} else {
			end_marker = event.endMarker;
		}
	}

	// decodeReplayStream calls decoder.finalize() which throws if header or end marker is missing.
	assert(header !== undefined, 'Header should have been parsed from stream');
	assert(end_marker !== undefined, 'End marker should have been parsed from stream');

	return {
		header,
		ticks,
		endMarker: end_marker,
	};
}
