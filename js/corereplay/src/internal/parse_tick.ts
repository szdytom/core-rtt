import { ReplayDecodeError } from '../errors.js';
import type {
	ReplayBullet,
	ReplayLogEntry,
	ReplayPlayer,
	ReplayTickFrame,
	ReplayUnit,
} from '../types.js';
import { ByteReader } from './byte_reader.js';

export interface TickParseResult {
	tick: ReplayTickFrame;
	consumed: number;
}

export interface TickParseLimits {
	maxUnitsPerTick: number;
	maxBulletsPerTick: number;
	maxLogsPerTick: number;
	maxLogPayloadSize: number;
}

const default_limits: TickParseLimits = {
	maxUnitsPerTick: 30,
	maxBulletsPerTick: 4096,
	maxLogsPerTick: 64,
	maxLogPayloadSize: 512,
};

export function parseTickPayload(
	bytes: Uint8Array,
	tick_id: number,
	absolute_offset: number,
	limits: TickParseLimits = default_limits,
): ReplayTickFrame {
	const reader = new ByteReader(bytes, absolute_offset);

	const p1: ReplayPlayer = {
		id: reader.readU8(),
		baseEnergy: reader.readU16(),
		baseCaptureCounter: reader.readU8(),
	};
	const p2: ReplayPlayer = {
		id: reader.readU8(),
		baseEnergy: reader.readU16(),
		baseCaptureCounter: reader.readU8(),
	};

	const unit_count = reader.readU16();
	if (unit_count > limits.maxUnitsPerTick) {
		throw new ReplayDecodeError(
			'UNIT_COUNT_EXCEEDS_MAXIMUM',
			absolute_offset + reader.cursor() - 2,
		);
	}

	const units: ReplayUnit[] = [];
	units.length = unit_count;
	for (let i = 0; i < unit_count; i += 1) {
		units[i] = {
			id: reader.readU8(),
			playerId: reader.readU8(),
			x: reader.readI16(),
			y: reader.readI16(),
			health: reader.readU8(),
			energy: reader.readU16(),
			attackCooldown: reader.readU8(),
			upgrades: reader.readU8(),
		};
	}

	const bullet_count = reader.readU16();
	if (bullet_count > limits.maxBulletsPerTick) {
		throw new ReplayDecodeError(
			'BULLET_COUNT_EXCEEDS_MAXIMUM',
			absolute_offset + reader.cursor() - 2,
		);
	}

	const bullets: ReplayBullet[] = [];
	bullets.length = bullet_count;
	for (let i = 0; i < bullet_count; i += 1) {
		bullets[i] = {
			x: reader.readI16(),
			y: reader.readI16(),
			direction: reader.readU8(),
			playerId: reader.readU8(),
			damage: reader.readU8(),
		};
	}

	const log_count = reader.readU16();
	if (log_count > limits.maxLogsPerTick) {
		throw new ReplayDecodeError(
			'LOG_COUNT_EXCEEDS_MAXIMUM',
			absolute_offset + reader.cursor() - 2,
		);
	}

	const logs: ReplayLogEntry[] = [];
	logs.length = log_count;
	for (let i = 0; i < log_count; i += 1) {
		logs[i] = parseLogEntry(reader, absolute_offset, limits.maxLogPayloadSize);
	}

	return {
		tick: tick_id,
		players: [p1, p2],
		units,
		bullets,
		logs,
	};
}

function parseLogEntry(reader: ByteReader, absolute_offset: number, max_log_payload_size: number): ReplayLogEntry {
	const tick = reader.readU32();
	const player_id = reader.readU8();
	const unit_id = reader.readU8();

	const source = reader.readU8();
	if (source > 1) {
		throw new ReplayDecodeError(
			'INVALID_LOG_SOURCE_VALUE',
			absolute_offset + reader.cursor() - 1,
		);
	}

	const type = reader.readU8();
	if (type > 4) {
		throw new ReplayDecodeError(
			'INVALID_LOG_TYPE_VALUE',
			absolute_offset + reader.cursor() - 1,
		);
	}

	const payload_size = reader.readU16();
	if (payload_size > max_log_payload_size) {
		throw new ReplayDecodeError(
			'LOG_PAYLOAD_SIZE_EXCEEDS_MAXIMUM',
			absolute_offset + reader.cursor() - 2,
		);
	}

	return {
		tick,
		playerId: player_id,
		unitId: unit_id,
		source: source as 0 | 1,
		type: type as 0 | 1 | 2 | 3 | 4,
		payload: reader.readBytes(payload_size),
	};
}
