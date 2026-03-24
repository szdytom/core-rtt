import { ReplayDecodeError } from '../errors.js';
import type { ReplayEndMarker } from '../types.js';
import { ByteReader } from './byte_reader.js';

export interface EndMarkerParseResult {
	endMarker: ReplayEndMarker;
	consumed: number;
}

export function parseEndMarkerAt(
	bytes: Uint8Array,
	offset: number,
): EndMarkerParseResult {
	const reader = new ByteReader(bytes.subarray(offset), offset);

	const raw_termination = reader.readU8();
	if (raw_termination > 1) {
		throw new ReplayDecodeError(
			'INVALID_REPLAY_TERMINATION',
			offset + reader.cursor() - 1,
		);
	}

	const winner_player_id = reader.readU8();

	let termination: ReplayEndMarker['termination'];
	if (raw_termination === 0) {
		termination = 'completed';
		if (winner_player_id === 0 || winner_player_id > 2) {
			throw new ReplayDecodeError('INVALID_END_MARKER_PAYLOAD', offset + 1);
		}
	} else {
		termination = 'aborted';
		if (winner_player_id !== 0) {
			throw new ReplayDecodeError('INVALID_END_MARKER_PAYLOAD', offset + 1);
		}
	}

	return {
		endMarker: {
			termination,
			winnerPlayerId: winner_player_id as 0 | 1 | 2,
		},
		consumed: 2,
	};
}
