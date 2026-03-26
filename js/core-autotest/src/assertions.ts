import type { ReplayLogEntry, ReplayLogSource, ReplayLogType } from '@corertt/corereplay';
import type { CaseSpec, FailureDetail, LogFilter, PayloadMatcher } from './types.js';

export function parseSource(value: number | string): ReplayLogSource {
	if (typeof value === 'number') {
		if (value === 0 || value === 1) {
			return value;
		}
		throw new Error(`Invalid log source value: ${value}`);
	}

	const normalized = value.toLowerCase();
	if (normalized === 'system' || normalized === 'sys') {
		return 0;
	}
	if (normalized === 'player' || normalized === 'usr' || normalized === 'user') {
		return 1;
	}
	throw new Error(`Invalid log source string: ${value}`);
}

export function parseType(value: number | string): ReplayLogType {
	if (typeof value === 'number') {
		if (value >= 0 && value <= 4) {
			return value as ReplayLogType;
		}
		throw new Error(`Invalid log type value: ${value}`);
	}

	const normalized = value.toLowerCase();
	if (normalized === 'custom') {
		return 0;
	}
	if (normalized === 'unitcreation' || normalized === 'unit_creation') {
		return 1;
	}
	if (normalized === 'unitdestruction' || normalized === 'unit_destruction') {
		return 2;
	}
	if (normalized === 'executionexception' || normalized === 'execution_exception') {
		return 3;
	}
	if (normalized === 'basecaptured' || normalized === 'base_captured') {
		return 4;
	}
	throw new Error(`Invalid log type string: ${value}`);
}

export function decodePayload(payload: Uint8Array): string {
	return Buffer.from(payload).toString('latin1');
}

export function matchesPayload(payload_text: string, matcher: string | PayloadMatcher): boolean {
	if (typeof matcher === 'string') {
		return payload_text === matcher;
	}

	if (matcher.equals != null && payload_text !== matcher.equals) {
		return false;
	}
	if (matcher.regex != null) {
		const regex = new RegExp(matcher.regex);
		if (!regex.test(payload_text)) {
			return false;
		}
	}
	return true;
}

export function matchesFilter(log_entry: ReplayLogEntry, filter: LogFilter): boolean {
	if (filter.source != null && log_entry.source !== parseSource(filter.source)) {
		return false;
	}
	if (filter.type != null && log_entry.type !== parseType(filter.type)) {
		return false;
	}
	if (filter.playerId != null && log_entry.playerId !== filter.playerId) {
		return false;
	}
	if (filter.unitId != null && log_entry.unitId !== filter.unitId) {
		return false;
	}
	if (filter.payload != null && !matchesPayload(decodePayload(log_entry.payload), filter.payload)) {
		return false;
	}
	return true;
}

function sourceToString(source: ReplayLogSource): string {
	return source === 0 ? 'System' : 'Player';
}

function typeToString(type: ReplayLogType): string {
	switch (type) {
		case 0:
			return 'Custom';
		case 1:
			return 'UnitCreation';
		case 2:
			return 'UnitDestruction';
		case 3:
			return 'ExecutionException';
		case 4:
			return 'BaseCaptured';
		default:
			return 'Unknown';
	}
}

export function formatLog(log_entry: ReplayLogEntry): string {
	return `tick=${log_entry.tick} player=${log_entry.playerId} unit=${log_entry.unitId} source=${sourceToString(log_entry.source)} type=${typeToString(log_entry.type)} payload=${JSON.stringify(decodePayload(log_entry.payload))}`;
}

export function evaluateAssertions(log_entries: ReplayLogEntry[], spec: CaseSpec): FailureDetail | undefined {
	for (const [index, assertion] of (spec.expectedLogs ?? []).entries()) {
		const count = log_entries.filter((entry) => matchesFilter(entry, assertion.filter)).length;
		const expected_min = assertion.atLeast ?? 1;
		if (count < expected_min) {
			return {
				message: `expectedLogs[${index}] matched ${count}, expected at least ${expected_min}`,
			};
		}
	}

	for (const [index, assertion] of (spec.forbiddenLogs ?? []).entries()) {
		const matched = log_entries.filter((entry) => matchesFilter(entry, assertion.filter));
		const max_allowed = assertion.maxAllowed ?? 0;
		if (matched.length > max_allowed) {
			return {
				message: `forbiddenLogs[${index}] matched ${matched.length}, max allowed ${max_allowed}`,
				offending_log: matched[0],
			};
		}
	}

	return undefined;
}
