import type { DecodeOptions, ReplayStreamInput } from './api.js';
import { ReplayDecoderCore } from './decoder_core.js';
import { ReplayDecodeError } from './errors.js';
import { decodeReplayStream } from './stream.js';
import type { ReplayData, ReplayInput } from './types.js';

export function decodeReplay(
	input: ReplayInput,
	options: DecodeOptions = {},
): ReplayData {
	const decoder = new ReplayDecoderCore(options);
	decoder.push(input);

	let header: ReplayData['header'] | undefined;
	let end_marker: ReplayData['endMarker'] | undefined;
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
		end_marker = result.endMarker;
		break;
	}

	if (header == null) {
		throw new ReplayDecodeError('MISSING_HEADER', decoder.position());
	}
	if (end_marker == null) {
		throw new ReplayDecodeError('MISSING_END_MARKER', decoder.position());
	}

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
			continue;
		}
		if (event.kind === 'tick') {
			ticks.push(event.tick);
			continue;
		}
		end_marker = event.endMarker;
	}

	// decodeReplayStream calls decoder.finalize() which throws if header or end marker is missing.
	// These checks are defensive guards that should not normally be reached.
	if (header == null) {
		throw new ReplayDecodeError('MISSING_HEADER', 0);
	}
	if (end_marker == null) {
		throw new ReplayDecodeError('MISSING_END_MARKER', 0);
	}

	return {
		header,
		ticks,
		endMarker: end_marker,
	};
}
