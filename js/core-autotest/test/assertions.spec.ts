import { describe, expect, test } from 'vitest';
import type { ReplayLogEntry } from '@corertt/corereplay';
import { evaluateAssertions, matchesFilter, matchesPayload, parseSource, parseType } from '../src/assertions.js';
import type { CaseSpec } from '../src/types.js';

function makeLog(overrides: Partial<ReplayLogEntry> = {}): ReplayLogEntry {
	return {
		tick: 1,
		playerId: 0,
		unitId: 0,
		source: 0,
		type: 0,
		payload: Buffer.from('PASS', 'latin1'),
		...overrides,
	};
}

function baseSpec(): CaseSpec {
	return {
		version: 1,
		name: 'x',
		timeLimitMs: 100,
		maxTicks: 10,
		program: [
			{ base: '@dummy', unit: '@dummy' },
			{ base: '@dummy', unit: '@dummy' },
		],
	};
}

describe('assertions', () => {
	test('parses source and type aliases', () => {
		expect(parseSource('System')).toBe(0);
		expect(parseSource('user')).toBe(1);
		expect(parseType('unit_creation')).toBe(1);
		expect(parseType('ExecutionException')).toBe(3);
	});

	test('matches payload with equals and regex', () => {
		expect(matchesPayload('PASS', 'PASS')).toBe(true);
		expect(matchesPayload('PASS', { equals: 'PASS' })).toBe(true);
		expect(matchesPayload('PASS42', { regex: '^PASS\\d+$' })).toBe(true);
		expect(matchesPayload('FAIL', { equals: 'PASS', regex: '^PASS$' })).toBe(false);
	});

	test('matches filter on structured fields', () => {
		const log = makeLog({ source: 1, type: 3, playerId: 1, unitId: 9, payload: Buffer.from('ABORTED') });
		expect(matchesFilter(log, { source: 'Player', type: 'ExecutionException', playerId: 1, unitId: 9, payload: 'ABORTED' })).toBe(true);
		expect(matchesFilter(log, { source: 'System' })).toBe(false);
	});

	test('returns failure detail for unmet expected assertion', () => {
		const spec = baseSpec();
		spec.expectedLogs = [{ filter: { payload: 'NEVER' }, atLeast: 1 }];
		const failure = evaluateAssertions([makeLog()], spec);
		expect(failure?.message).toContain('expectedLogs[0]');
	});

	test('returns offending log for forbidden assertion', () => {
		const spec = baseSpec();
		spec.forbiddenLogs = [{ filter: { payload: 'PASS' }, maxAllowed: 0 }];
		const failure = evaluateAssertions([makeLog()], spec);
		expect(failure?.message).toContain('forbiddenLogs[0]');
		expect(failure?.offending_log).toBeDefined();
	});
});
