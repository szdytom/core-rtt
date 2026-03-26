import type { DecodeOptions, InternalDecoder, InternalReadResult } from './api.js';
import { ReplayDecodeError } from './errors.js';
import type { ReplayEndMarker, ReplayHeader, ReplayInput } from './types.js';
import { parseEndMarkerAt } from './internal/parse_end.js';
import { parseHeaderAt } from './internal/parse_header.js';
import { parseTickPayload, type TickParseLimits } from './internal/parse_tick.js';
import assert from 'assert';

enum ParsePhase {
	Header = 0,
	Tick = 1,
	End = 2,
}

const record_type_tick = 1;
const record_type_end = 255;

export class ReplayDecoderCore implements InternalDecoder {
	private readonly limits: TickParseLimits;
	private readonly is_strict: boolean;
	private phase: ParsePhase = ParsePhase.Header;
	private buffer = new Uint8Array(0);
	private cursor = 0;
	private buffer_start = 0; // absolute byte offset of buffer[0]
	private header_value: ReplayHeader | undefined;
	private end_marker: ReplayEndMarker | undefined;

	constructor(options: DecodeOptions = {}) {
		this.is_strict = options.strict ?? true;
		this.limits = {
			maxUnitsPerTick: options.maxUnitsPerTick ?? 30,
			maxBulletsPerTick: options.maxBulletsPerTick ?? 4096,
			maxLogsPerTick: options.maxLogsPerTick ?? 64,
			maxLogPayloadSize: options.maxLogPayloadSize ?? 512,
		};
	}

	/** Returns the current absolute byte offset from the start of the stream. */
	position(): number {
		return this.buffer_start + this.cursor;
	}

	push(input: ReplayInput): void {
		const chunk = toUint8Array(input);
		if (chunk.length === 0) {
			return;
		}

		const unread = this.buffer.subarray(this.cursor);
		const merged = new Uint8Array(unread.length + chunk.length);
		merged.set(unread, 0);
		merged.set(chunk, unread.length);
		this.buffer = merged;
		this.cursor = 0;
	}

	read(): InternalReadResult {
		if (this.phase === ParsePhase.End) {
			if (this.end_marker === undefined) {
				throw new ReplayDecodeError('MISSING_END_MARKER', this.position());
			}
			return { kind: 'end', endMarker: this.end_marker };
		}

		if (this.phase === ParsePhase.Header) {
			if (this.buffer.length - this.cursor < 8) {
				return { kind: 'need-more-data' };
			}

			const header_size = this.peekU16(this.cursor + 6);
			const total_size = 8 + header_size;
			if (this.buffer.length - this.cursor < total_size) {
				return { kind: 'need-more-data' };
			}

			const parsed = parseHeaderAt(this.buffer, this.cursor);
			this.header_value = parsed.header;
			this.cursor += parsed.consumed;
			this.phase = ParsePhase.Tick;
			this.compact();
			return { kind: 'header', header: parsed.header };
		}

		if (this.buffer.length - this.cursor < 1) {
			return { kind: 'need-more-data' };
		}

		const record_type = this.buffer[this.cursor];
		if (record_type === record_type_end) {
			if (this.buffer.length - this.cursor < 3) {
				return { kind: 'need-more-data' };
			}

			const parsed = parseEndMarkerAt(this.buffer, this.cursor + 1);
			this.end_marker = parsed.endMarker;
			this.cursor += 1 + parsed.consumed;
			this.phase = ParsePhase.End;
			this.compact();
			return { kind: 'end', endMarker: parsed.endMarker };
		}

		if (record_type !== record_type_tick) {
			throw new ReplayDecodeError('UNKNOWN_RECORD_TYPE', this.position());
		}

		if (this.buffer.length - this.cursor < 9) {
			return { kind: 'need-more-data' };
		}

		const tick_id = this.peekU32(this.cursor + 1);
		const payload_size = this.peekU32(this.cursor + 5);
		const record_size = 9 + payload_size;

		if (this.buffer.length - this.cursor < record_size) {
			return { kind: 'need-more-data' };
		}

		const payload_begin = this.cursor + 9;
		const payload_end = payload_begin + payload_size;
		const payload = this.buffer.subarray(payload_begin, payload_end);

		const tick = parseTickPayload(
			payload,
			tick_id,
			this.buffer_start + payload_begin,
			this.limits,
		);

		this.cursor = payload_end;
		this.compact();
		return { kind: 'tick', tick };
	}

	state(): {
		hasHeader: boolean;
		ended: boolean;
		header?: ReplayHeader;
		endMarker?: ReplayEndMarker;
	} {
		return {
			hasHeader: this.header_value !== undefined,
			ended: this.phase === ParsePhase.End,
			header: this.header_value,
			endMarker: this.end_marker,
		};
	}

	finalize(): ReplayEndMarker {
		if (this.phase === ParsePhase.Header) {
			throw new ReplayDecodeError('MISSING_HEADER', this.position());
		}

		if (this.phase === ParsePhase.End) {
			assert(this.end_marker !== undefined);
			return this.end_marker;
		}

		assert(this.phase === ParsePhase.Tick);
		if (this.is_strict) {
			throw new ReplayDecodeError('MISSING_END_MARKER', this.position());
		}

		return this.synthesizeAbortedEndMarker();
	}

	private synthesizeAbortedEndMarker(): ReplayEndMarker {
		this.phase = ParsePhase.End;
		this.end_marker = {
			termination: 'aborted',
			winnerPlayerId: 0,
		};
		return this.end_marker;
	}

	private compact(): void {
		if (this.cursor === 0) {
			return;
		}

		if (this.cursor * 2 >= this.buffer.length) {
			this.buffer_start += this.cursor;
			this.buffer = this.buffer.subarray(this.cursor);
			this.cursor = 0;
		}
	}

	private peekU16(index: number): number {
		return this.buffer[index] | (this.buffer[index + 1] << 8);
	}

	private peekU32(index: number): number {
		return (
			this.buffer[index]
			| (this.buffer[index + 1] << 8)
			| (this.buffer[index + 2] << 16)
			| (this.buffer[index + 3] << 24)
		) >>> 0;
	}
}

function toUint8Array(input: ReplayInput): Uint8Array {
	if (input instanceof Uint8Array) {
		return input;
	}
	return new Uint8Array(input);
}
