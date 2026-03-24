import { ReplayDecodeError } from '../errors.js';
import type { ReplayHeader, ReplayTile, ReplayTilemap } from '../types.js';
import { ByteReader } from './byte_reader.js';

export interface HeaderParseResult {
	header: ReplayHeader;
	consumed: number;
}

const replay_magic = [0x43, 0x52, 0x50, 0x4c] as const;
const replay_version = 4;
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
	const tilemap = parseTilemap(header_payload, offset + 8);

	const header: ReplayHeader = {
		magic: 'CRPL',
		version: 4,
		headerSize: header_size,
		tilemap,
	};

	return { header, consumed: 8 + header_size };
}

function parseTilemap(bytes: Uint8Array, absolute_offset: number): ReplayTilemap {
	const reader = new ByteReader(bytes, absolute_offset);

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
