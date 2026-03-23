export type ReplayDecodeErrorCode =
	| 'TRUNCATED_INPUT'
	| 'BAD_MAGIC'
	| 'UNSUPPORTED_VERSION'
	| 'TRUNCATED_HEADER_PAYLOAD'
	| 'TRUNCATED_TILEMAP_HEADER'
	| 'TILEMAP_DIMENSIONS_EXCEED_MAXIMUM'
	| 'TILEMAP_TILE_COUNT_EXCEEDS_MAXIMUM'
	| 'TILEMAP_DATA_TRUNCATED'
	| 'TRUNCATED_LOG_ENTRY'
	| 'INVALID_LOG_SOURCE_VALUE'
	| 'INVALID_LOG_TYPE_VALUE'
	| 'LOG_PAYLOAD_TRUNCATED'
	| 'TICK_PAYLOAD_TOO_SMALL'
	| 'UNIT_COUNT_EXCEEDS_MAXIMUM'
	| 'BULLET_COUNT_EXCEEDS_MAXIMUM'
	| 'LOG_COUNT_EXCEEDS_MAXIMUM'
	| 'LOG_PAYLOAD_SIZE_EXCEEDS_MAXIMUM'
	| 'TRUNCATED_END_MARKER'
	| 'INVALID_REPLAY_TERMINATION'
	| 'INVALID_END_MARKER_PAYLOAD'
	| 'UNKNOWN_RECORD_TYPE'
	| 'MISSING_HEADER'
	| 'MISSING_END_MARKER';

const default_messages: Record<ReplayDecodeErrorCode, string> = {
	TRUNCATED_INPUT: 'Replay decode failed: truncated input',
	BAD_MAGIC: 'Replay decode failed: bad magic',
	UNSUPPORTED_VERSION: 'Replay decode failed: unsupported version',
	TRUNCATED_HEADER_PAYLOAD: 'Replay decode failed: truncated header payload',
	TRUNCATED_TILEMAP_HEADER: 'Replay decode failed: truncated tilemap header',
	TILEMAP_DIMENSIONS_EXCEED_MAXIMUM:
		'Replay decode failed: tilemap dimensions exceed maximum',
	TILEMAP_TILE_COUNT_EXCEEDS_MAXIMUM:
		'Replay decode failed: tilemap tile count exceeds maximum',
	TILEMAP_DATA_TRUNCATED: 'Replay decode failed: tilemap data truncated',
	TRUNCATED_LOG_ENTRY: 'Replay decode failed: truncated log entry',
	INVALID_LOG_SOURCE_VALUE: 'Replay decode failed: invalid log source value',
	INVALID_LOG_TYPE_VALUE: 'Replay decode failed: invalid log type value',
	LOG_PAYLOAD_TRUNCATED: 'Replay decode failed: log payload truncated',
	TICK_PAYLOAD_TOO_SMALL: 'Replay decode failed: tick payload too small',
	UNIT_COUNT_EXCEEDS_MAXIMUM:
		'Replay decode failed: unit count exceeds maximum',
	BULLET_COUNT_EXCEEDS_MAXIMUM:
		'Replay decode failed: bullet count exceeds maximum',
	LOG_COUNT_EXCEEDS_MAXIMUM: 'Replay decode failed: log count exceeds maximum',
	LOG_PAYLOAD_SIZE_EXCEEDS_MAXIMUM:
		'Replay decode failed: log payload size exceeds maximum',
	TRUNCATED_END_MARKER: 'Replay decode failed: truncated end marker',
	INVALID_REPLAY_TERMINATION:
		'Replay decode failed: invalid replay termination',
	INVALID_END_MARKER_PAYLOAD:
		'Replay decode failed: invalid end marker payload',
	UNKNOWN_RECORD_TYPE: 'Replay decode failed: unknown record type',
	MISSING_HEADER: 'Replay decode failed: missing header',
	MISSING_END_MARKER: 'Replay decode failed: missing end marker',
};

export class ReplayDecodeError extends Error {
	readonly code: ReplayDecodeErrorCode;
	readonly offset: number;
	readonly fatal: boolean;

	constructor(
		code: ReplayDecodeErrorCode,
		offset: number,
		message?: string,
		fatal = true,
	) {
		super(message ?? default_messages[code]);
		this.name = 'ReplayDecodeError';
		this.code = code;
		this.offset = offset;
		this.fatal = fatal;
	}
}
