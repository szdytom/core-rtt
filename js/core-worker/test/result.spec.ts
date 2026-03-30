import { describe, expect, test } from 'vitest';
import { MatchResult, TaskStatus, StrategyGroupDescriptor } from '@corertt/worker-codec';
import { buildTaskCoreCrashResult, buildTaskSuccessResult } from '../src/result.js';

function createStrategies(): [StrategyGroupDescriptor, StrategyGroupDescriptor] {
	const p1 = new StrategyGroupDescriptor();
	p1.strategyGroupId = 101;
	p1.baseLastModified = new Date('2026-01-01T00:00:00.000Z');
	p1.unitLastModified = new Date('2026-01-01T00:00:00.000Z');
	p1.baseStrategyUrl = 'https://example.invalid/p1/base';
	p1.unitStrategyUrl = 'https://example.invalid/p1/unit';

	const p2 = new StrategyGroupDescriptor();
	p2.strategyGroupId = 202;
	p2.baseLastModified = new Date('2026-01-01T00:00:00.000Z');
	p2.unitLastModified = new Date('2026-01-01T00:00:00.000Z');
	p2.baseStrategyUrl = 'https://example.invalid/p2/base';
	p2.unitStrategyUrl = 'https://example.invalid/p2/unit';

	return [p1, p2];
}

describe('result builders', () => {
	test('buildTaskSuccessResult uses completed termination jsonl winner and crash flags', () => {
		const stderrText = [
			'{"version":"0.1","commit":"abc","zstd_version":10507}',
			'{"termination":"completed","winner_player_id":2,"p1_base_crash":true,"p1_unit_crash":false,"p2_base_crash":false,"p2_unit_crash":true}',
		].join('\n');

		const packet = buildTaskSuccessResult(42, createStrategies(), stderrText, 64 * 1024);

		expect(packet.matchId).toBe(42);
		expect(packet.status).toBe(TaskStatus.Success);
		expect(packet.result).toBe(MatchResult.P2Win);
		expect(packet.errorLog).toBe(stderrText);
		expect(packet.crashInfo[0].baseStrategyCrashed).toBe(true);
		expect(packet.crashInfo[0].unitStrategyCrashed).toBe(false);
		expect(packet.crashInfo[1].baseStrategyCrashed).toBe(false);
		expect(packet.crashInfo[1].unitStrategyCrashed).toBe(true);
	});

	test('buildTaskSuccessResult maps aborted termination to tied result', () => {
		const stderrText = '{"termination":"aborted","winner_player_id":0,"p1_base_crash":false,"p1_unit_crash":false,"p2_base_crash":false,"p2_unit_crash":false}';

		const packet = buildTaskSuccessResult(77, createStrategies(), stderrText, 64 * 1024);

		expect(packet.status).toBe(TaskStatus.Success);
		expect(packet.result).toBe(MatchResult.Tied);
	});

	test('buildTaskSuccessResult throws when termination jsonl is missing', () => {
		expect(() => buildTaskSuccessResult(99, createStrategies(), 'segmentation fault\nstacktrace...', 64 * 1024)).toThrow(/missing worker termination jsonl marker/);
	});

	test('buildTaskCoreCrashResult keeps raw stderr and still extracts crash flags when available', () => {
		const stderrText = [
			'AddressSanitizer: heap-use-after-free',
			'{"termination":"completed","winner_player_id":1,"p1_base_crash":false,"p1_unit_crash":true,"p2_base_crash":true,"p2_unit_crash":false}',
			'core dumped',
		].join('\n');

		const packet = buildTaskCoreCrashResult(55, createStrategies(), stderrText, 64 * 1024);

		expect(packet.status).toBe(TaskStatus.CoreCrash);
		expect(packet.result).toBe(MatchResult.NoResult);
		expect(packet.errorLog).toBe(stderrText);
		expect(packet.crashInfo[0].baseStrategyCrashed).toBe(false);
		expect(packet.crashInfo[0].unitStrategyCrashed).toBe(true);
		expect(packet.crashInfo[1].baseStrategyCrashed).toBe(true);
		expect(packet.crashInfo[1].unitStrategyCrashed).toBe(false);
	});
});
