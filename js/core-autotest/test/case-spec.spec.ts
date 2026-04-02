import { describe, expect, test } from 'vitest';
import { parseCaseSpec } from '../src/case-spec.js';

function validRawCase(): Record<string, unknown> {
	return {
		version: 1,
		name: 'sample case',
		timeLimitMs: 100,
		maxTicks: 50,
		program: [
			{ base: '@dummy', unit: '@dummy' },
			{ base: '@dummy', unit: '@dummy' },
		],
	};
}

describe('parseCaseSpec', () => {
	test('parses minimal valid case with defaults', () => {
		const spec = parseCaseSpec(validRawCase(), 'sample.json');
		expect(spec.runs).toBe(1);
		expect(spec.expectFailure).toBe(false);
		expect(spec.expectedLogs).toEqual([]);
		expect(spec.forbiddenLogs).toEqual([]);
	});

	test('rejects invalid version', () => {
		const raw = validRawCase();
		raw.version = 2;
		expect(() => parseCaseSpec(raw, 'sample.json')).toThrow('sample.json.version must be 1');
	});

	test('rejects malformed payload matcher', () => {
		const raw = validRawCase();
		raw.expectedLogs = [{ filter: { payload: {} } }];
		expect(() => parseCaseSpec(raw, 'sample.json')).toThrow('sample.json.expectedLogs[0].filter.payload must contain equals or regex');
	});

	test('parses expected and forbidden assertions', () => {
		const raw = validRawCase();
		raw.expectedLogs = [{ filter: { source: 'System', payload: { regex: 'PASS' } }, atLeast: 2 }];
		raw.forbiddenLogs = [{ filter: { type: 'ExecutionException' }, maxAllowed: 0 }];
		const spec = parseCaseSpec(raw, 'sample.json');
		expect(spec.expectedLogs?.[0].atLeast).toBe(2);
		expect(spec.forbiddenLogs?.[0].maxAllowed).toBe(0);
	});

	test('parses map field', () => {
		const raw = validRawCase();
		raw.map = 'tiny.txt';
		const spec = parseCaseSpec(raw, 'sample.json');
		expect(spec.map).toBe('tiny.txt');
	});

	test('parses gameRules overrides', () => {
		const raw = validRawCase();
		raw.gameRules = {
			unitHealth: 120,
			attackCooldown: 4,
			manufactCost: 750,
		};
		const spec = parseCaseSpec(raw, 'sample.json');
		expect(spec.gameRules).toEqual({
			unitHealth: 120,
			attackCooldown: 4,
			manufactCost: 750,
		});
	});

	test('rejects layout gameRules with map', () => {
		const raw = validRawCase();
		raw.map = 'tiny.txt';
		raw.gameRules = { width: 32 };
		expect(() => parseCaseSpec(raw, 'sample.json')).toThrow(
			'sample.json.gameRules.width, sample.json.gameRules.height, and sample.json.gameRules.baseSize cannot be used with sample.json.map',
		);
	});

	test('rejects map and seed together', () => {
		const raw = validRawCase();
		raw.map = 'tiny.txt';
		raw.seed = '12345';
		expect(() => parseCaseSpec(raw, 'sample.json')).toThrow('sample.json.seed and sample.json.map are mutually exclusive');
	});
});
