import { ReplayDecodeError } from '../errors.js';

export class ByteReader {
	private readonly bytes: Uint8Array;
	private readonly view: DataView;
	private cursor_value = 0;
	private readonly base_offset: number;

	constructor(bytes: Uint8Array, base_offset = 0) {
		this.bytes = bytes;
		this.view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
		this.base_offset = base_offset;
	}

	cursor(): number {
		return this.cursor_value;
	}

	remaining(): number {
		return this.bytes.length - this.cursor_value;
	}

	has(size: number): boolean {
		return this.cursor_value + size <= this.bytes.length;
	}

	readU8(): number {
		if (this.cursor_value + 1 > this.bytes.length) {
			throw new ReplayDecodeError('TRUNCATED_REPLAY', this.base_offset + this.cursor_value);
		}
		const value = this.view.getUint8(this.cursor_value);
		this.cursor_value += 1;
		return value;
	}

	readU16(): number {
		if (this.cursor_value + 2 > this.bytes.length) {
			throw new ReplayDecodeError('TRUNCATED_REPLAY', this.base_offset + this.cursor_value);
		}
		const value = this.view.getUint16(this.cursor_value, true);
		this.cursor_value += 2;
		return value;
	}

	readI16(): number {
		if (this.cursor_value + 2 > this.bytes.length) {
			throw new ReplayDecodeError('TRUNCATED_REPLAY', this.base_offset + this.cursor_value);
		}
		const value = this.view.getInt16(this.cursor_value, true);
		this.cursor_value += 2;
		return value;
	}

	readU32(): number {
		if (this.cursor_value + 4 > this.bytes.length) {
			throw new ReplayDecodeError('TRUNCATED_REPLAY', this.base_offset + this.cursor_value);
		}
		const value = this.view.getUint32(this.cursor_value, true);
		this.cursor_value += 4;
		return value;
	}

	readBytes(size: number): Uint8Array {
		if (this.cursor_value + size > this.bytes.length) {
			throw new ReplayDecodeError('TRUNCATED_REPLAY', this.base_offset + this.cursor_value);
		}
		const begin = this.cursor_value;
		this.cursor_value += size;
		return this.bytes.subarray(begin, begin + size);
	}

	skip(size: number): void {
		if (this.cursor_value + size > this.bytes.length) {
			throw new ReplayDecodeError('TRUNCATED_REPLAY', this.base_offset + this.cursor_value);
		}
		this.cursor_value += size;
	}
}
