import type { ReplayLogEntry, ReplayLogSource, ReplayLogType } from '@corertt/corereplay';

export type OutputFormat = 'jsonl' | 'text';

export interface ReplayLogJsonLine {
	tick: number;
	player_id: number;
	unit_id: number;
	source: 'SYS' | 'USR';
	type: 'custom' | 'unit_creation' | 'unit_destruction' | 'execution_exception' | 'base_captured';
	payload_text: string;
	payload_hex: string;
}

export function logSourceToString(source: ReplayLogSource): 'SYS' | 'USR' {
	switch (source) {
		case 0:
			return 'SYS';
		case 1:
			return 'USR';
		default:
			throw new Error('Invalid replay log source');
	}
}

export function logTypeToString(type: ReplayLogType): ReplayLogJsonLine['type'] {
	switch (type) {
		case 0:
			return 'custom';
		case 1:
			return 'unit_creation';
		case 2:
			return 'unit_destruction';
		case 3:
			return 'execution_exception';
		case 4:
			return 'base_captured';
		default:
			throw new Error('Invalid replay log type');
	}
}

export function payloadToHex(payload: Uint8Array): string {
	const digits = '0123456789ABCDEF';
	let hex = '';
	for (const value of payload) {
		hex += digits[(value >> 4) & 0x0f];
		hex += digits[value & 0x0f];
	}
	return hex;
}

export function payloadToText(payload: Uint8Array): string {
	return Buffer.from(payload).toString('latin1');
}

function isPrintableAscii(value: number): boolean {
	return value >= 0x20 && value <= 0x7e;
}

function computePrefix(entry: ReplayLogEntry): string {
	const dev_name = entry.unitId === 0
		? 'base'
		: entry.unitId.toString().padStart(2, '0');
	const tick_text = entry.tick.toString().padStart(4, '0');
	return `[${tick_text} P${entry.playerId}-${dev_name} ${logSourceToString(entry.source)}] `;
}

export function formatReplayLogEntryLines(entry: ReplayLogEntry): string[] {
	const lines: string[] = [];
	const prefix = computePrefix(entry);
	const indent = ' '.repeat(prefix.length);
	const payload = entry.payload;
	let payload_pos = 0;
	let is_first_line = true;

	while (true) {
		let line = is_first_line ? prefix : indent;
		is_first_line = false;

		while (payload_pos < payload.length) {
			const value = payload[payload_pos];
			payload_pos += 1;

			if (value === 0x0a) {
				break;
			}

			if (value === 0x09 || value === 0x0d || isPrintableAscii(value)) {
				line += String.fromCharCode(value);
				continue;
			}

			line += `\\x${value.toString(16).toUpperCase().padStart(2, '0')}`;
		}

		lines.push(line);
		if (payload_pos >= payload.length) {
			break;
		}
	}

	return lines;
}

export function toReplayLogJsonLine(entry: ReplayLogEntry): ReplayLogJsonLine {
	return {
		payload_hex: payloadToHex(entry.payload),
		payload_text: payloadToText(entry.payload),
		player_id: entry.playerId,
		source: logSourceToString(entry.source),
		tick: entry.tick,
		type: logTypeToString(entry.type),
		unit_id: entry.unitId,
	};
}
