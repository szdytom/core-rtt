import { ReplayDecodeError } from '../errors.js';
import type {
	ReplayGameRules,
	ReplayHeader,
	ReplayTile,
	ReplayTilemap,
} from '../types.js';
import { ByteReader } from './byte_reader.js';

export interface HeaderParseResult {
	header: ReplayHeader;
	consumed: number;
}

const replay_magic = [0x43, 0x52, 0x50, 0x4c] as const;
const replay_version = 5;
const max_tile_dimension = 256;
const max_tile_count = max_tile_dimension * max_tile_dimension;

export function parseHeaderAt(bytes: Uint8Array, offset: number): HeaderParseResult {
	const available = bytes.subarray(offset);
	const reader = new ByteReader(available, offset);

	for (const expected of replay_magic) {
		const actual = reader.readU8();
		if (actual !== expected) {
			throw new ReplayDecodeError('BAD_MAGIC', offset + reader.cursor() - 1);
		}
	}

	const version = reader.readU16();
	const header_size = reader.readU16();
	if (version !== replay_version) {
		throw new ReplayDecodeError('UNSUPPORTED_VERSION', offset + 4);
	}

	const header_payload = reader.readBytes(header_size);
	const payload_reader = new ByteReader(header_payload, offset + 8);
	const tilemap = parseTilemap(payload_reader, offset + 8);
	const game_rules = parseGameRules(payload_reader);

	const header: ReplayHeader = {
		magic: 'CRPL',
		version,
		headerSize: header_size,
		tilemap,
		gameRules: game_rules,
	};

	return { header, consumed: 8 + header_size };
}

function parseTilemap(reader: ByteReader, absolute_offset: number): ReplayTilemap {
	const width = reader.readU16();
	const height = reader.readU16();
	const base_size = reader.readU16();
	reader.skip(2);

	if (width > max_tile_dimension || height > max_tile_dimension) {
		throw new ReplayDecodeError(
			'TILEMAP_DIMENSIONS_EXCEED_MAXIMUM',
			absolute_offset,
		);
	}

	const tile_count = width * height;
	if (tile_count > max_tile_count) {
		throw new ReplayDecodeError(
			'TILEMAP_TILE_COUNT_EXCEEDS_MAXIMUM',
			absolute_offset,
		);
	}

	const tiles: ReplayTile[] = [];
	tiles.length = tile_count;

	for (let i = 0; i < tile_count; i += 1) {
		const packed = reader.readU8();
		tiles[i] = {
			terrain: packed & 0b11,
			side: (packed >> 2) & 0b11,
			isResource: (packed & 0b00010000) !== 0,
			isBase: (packed & 0b00100000) !== 0,
		};
	}

	return {
		width,
		height,
		baseSize: base_size,
		tiles,
	};
}

function parseGameRules(reader: ByteReader): ReplayGameRules {
	return {
		width: reader.readU8(),
		height: reader.readU8(),
		baseSize: reader.readU8(),
		unitHealth: reader.readU8(),
		naturalEnergyRate: reader.readU8(),
		resourceZoneEnergyRate: reader.readU8(),
		attackCooldown: reader.readU8(),
		captureTurnThreshold: reader.readU8(),
		visionLv1: reader.readU8(),
		visionLv2: reader.readU8(),
		capacityLv1: reader.readU16(),
		capacityLv2: reader.readU16(),
		capacityUpgradeCost: reader.readU16(),
		visionUpgradeCost: reader.readU16(),
		damageUpgradeCost: reader.readU16(),
		manufactCost: reader.readU16(),
	};
}
