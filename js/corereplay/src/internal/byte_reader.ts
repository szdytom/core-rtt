export class ByteReader {
	private readonly bytes: Uint8Array;
	private readonly view: DataView;
	private cursor_value = 0;

	constructor(bytes: Uint8Array) {
		this.bytes = bytes;
		this.view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
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
		const value = this.view.getUint8(this.cursor_value);
		this.cursor_value += 1;
		return value;
	}

	readU16(): number {
		const value = this.view.getUint16(this.cursor_value, true);
		this.cursor_value += 2;
		return value;
	}

	readI16(): number {
		const value = this.view.getInt16(this.cursor_value, true);
		this.cursor_value += 2;
		return value;
	}

	readU32(): number {
		const value = this.view.getUint32(this.cursor_value, true);
		this.cursor_value += 4;
		return value;
	}

	readBytes(size: number): Uint8Array {
		const begin = this.cursor_value;
		this.cursor_value += size;
		return this.bytes.subarray(begin, begin + size);
	}

	skip(size: number): boolean {
		if (!this.has(size)) {
			return false;
		}
		this.cursor_value += size;
		return true;
	}
}
