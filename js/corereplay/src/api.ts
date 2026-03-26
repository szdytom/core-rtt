import type { ReplayDecodeErrorCode } from './errors.js';
import type {
	ReplayData,
	ReplayEndMarker,
	ReplayHeader,
	ReplayInput,
	ReplayTickFrame,
} from './types.js';

export interface DecodeOptions {
	/**
	 * Decoding strictness.
	 * - `true` (default): replay must contain a full end marker.
	 * - `false`: tolerate EOF truncation and synthesize an aborted end marker.
	 */
	strict?: boolean;
	maxUnitsPerTick?: number;
	maxBulletsPerTick?: number;
	maxLogsPerTick?: number;
	maxLogPayloadSize?: number;
}

export type ReplayStreamInput =
	| AsyncIterable<ReplayInput>
	| ReadableStream<Uint8Array>;

export interface ReplayStreamHeaderEvent {
	kind: 'header';
	header: ReplayHeader;
}

export interface ReplayStreamTickEvent {
	kind: 'tick';
	tick: ReplayTickFrame;
}

export interface ReplayStreamEndEvent {
	kind: 'end';
	endMarker: ReplayEndMarker;
}

export type ReplayStreamEvent =
	| ReplayStreamHeaderEvent
	| ReplayStreamTickEvent
	| ReplayStreamEndEvent;

export interface ReplayDecoderState {
	hasHeader: boolean;
	ended: boolean;
	header?: ReplayHeader;
	endMarker?: ReplayEndMarker;
}

export type InternalReadResult =
	| { kind: 'need-more-data' }
	| { kind: 'header'; header: ReplayHeader }
	| { kind: 'tick'; tick: ReplayTickFrame }
	| { kind: 'end'; endMarker: ReplayEndMarker };

export interface InternalDecoder {
	push(input: ReplayInput): void;
	read(): InternalReadResult;
	state(): ReplayDecoderState;
	finalize(): ReplayEndMarker;
	position(): number;
}

export type ReplayDecodeFailure = {
	code: ReplayDecodeErrorCode;
	offset: number;
	message: string;
	fatal: boolean;
};

export type { ReplayData, ReplayInput };
