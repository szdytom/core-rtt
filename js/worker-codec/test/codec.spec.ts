import { describe, expect, test } from 'vitest';
import {
	decodePacket,
	encodePacket,
	MatchResult,
	MessageType,
	PROTOCOL_MAGIC,
	StrategyExecutionCrashInfo,
	StrategyGroupDescriptor,
	TaskAckownledgedPacket,
	TaskAssignPacket,
	TaskResultPacket,
	TaskStatus,
	winnerIndexOf,
} from '../src/index.js';

function createStrategyGroupDescriptor(
	strategyGroupId: string,
	baseStrategyUrl: string,
	unitStrategyUrl: string,
): StrategyGroupDescriptor {
	const descriptor = new StrategyGroupDescriptor();
	descriptor.strategyGroupId = strategyGroupId;
	descriptor.baseLastModified = new Date('2026-03-01T00:00:00.000Z');
	descriptor.unitLastModified = new Date('2026-03-01T00:00:00.000Z');
	descriptor.baseStrategyUrl = baseStrategyUrl;
	descriptor.unitStrategyUrl = unitStrategyUrl;
	return descriptor;
}

function createCrashInfo(
	strategyGroupId: string,
	baseStrategyCrashed: boolean,
	unitStrategyCrashed: boolean,
): StrategyExecutionCrashInfo {
	const crashInfo = new StrategyExecutionCrashInfo();
	crashInfo.strategyGroupId = strategyGroupId;
	crashInfo.baseStrategyCrashed = baseStrategyCrashed;
	crashInfo.unitStrategyCrashed = unitStrategyCrashed;
	return crashInfo;
}

describe('worker-codec', () => {
	test('round-trips TaskAssign packets', () => {
		const packet = new TaskAssignPacket();
		packet.matchId = '123456789012';
		packet.mapData = new Uint8Array([1, 2, 3, 4]).buffer;
		packet.strategies = [
			createStrategyGroupDescriptor('123456789013', 'https://example.com/base/a.ts', 'https://example.com/unit/a.ts'),
			createStrategyGroupDescriptor('123456789014', 'https://example.com/base/b.ts', 'https://example.com/unit/b.ts'),
		];
		packet.replayUploadUrl = 'https://example.com/replays/42';

		const encoded = encodePacket(packet);
		const decoded = decodePacket(encoded);

		expect(decoded).toBeInstanceOf(TaskAssignPacket);
		const decodedAssign = decoded as TaskAssignPacket;
		expect(decodedAssign.matchId).toBe('123456789012');
		expect(Array.from(new Uint8Array(decodedAssign.mapData))).toEqual([1, 2, 3, 4]);
		expect(decodedAssign.strategies[0].strategyGroupId).toBe('123456789013');
		expect(decodedAssign.strategies[1].strategyGroupId).toBe('123456789014');
		expect(decodedAssign.replayUploadUrl).toBe('https://example.com/replays/42');
	});

	test('round-trips TaskResult packets including errorLog', () => {
		const packet = new TaskResultPacket();
		packet.matchId = '123456789015';
		packet.result = MatchResult.P2Win;
		packet.status = TaskStatus.CoreCrash;
		packet.errorLog = 'segfault in core';
		packet.crashInfo = [
			createCrashInfo('123456789016', true, false),
			createCrashInfo('123456789017', false, true),
		];
		packet.finishedAt = new Date('2026-03-01T12:30:00.000Z');

		const encoded = encodePacket(packet);
		const decoded = decodePacket(encoded);

		expect(decoded).toBeInstanceOf(TaskResultPacket);
		const decodedResult = decoded as TaskResultPacket;
		expect(decodedResult.matchId).toBe('123456789015');
		expect(decodedResult.result).toBe(MatchResult.P2Win);
		expect(decodedResult.status).toBe(TaskStatus.CoreCrash);
		expect(decodedResult.errorLog).toBe('segfault in core');
		expect(decodedResult.crashInfo[0].baseStrategyCrashed).toBe(true);
		expect(decodedResult.crashInfo[1].unitStrategyCrashed).toBe(true);
		expect(decodedResult.finishedAt.getTime()).toBe(packet.finishedAt.getTime());
	});

	test('throws for invalid protocol magic', () => {
		const packet = new TaskAckownledgedPacket();
		packet.matchId = '123456789018';
		const encoded = encodePacket(packet);
		const bytes = new Uint8Array(encoded);

		bytes[0] = 0;
		bytes[1] = 0;
		expect(() => decodePacket(bytes.buffer)).toThrowError('Invalid protocol magic');
	});

	test('throws for unsupported codec version', () => {
		const packet = new TaskAckownledgedPacket();
		packet.matchId = '123456789019';
		const encoded = encodePacket(packet);
		const bytes = new Uint8Array(encoded);

		bytes[2] = 2;
		expect(() => decodePacket(bytes.buffer)).toThrowError('Unsupported codec version');
	});

	test('throws when packet body is truncated', () => {
		const packet = new TaskAssignPacket();
		packet.matchId = '123456789020';
		packet.mapData = new Uint8Array([7, 7, 7]).buffer;
		packet.strategies = [
			createStrategyGroupDescriptor('123456789021', 'a', 'b'),
			createStrategyGroupDescriptor('123456789022', 'c', 'd'),
		];
		packet.replayUploadUrl = 'https://example.com/upload';
		const encoded = encodePacket(packet);

		const truncated = encoded.slice(0, encoded.byteLength - 2);
		expect(() => decodePacket(truncated)).toThrowError('Buffer does not contain full packet');
	});

	test('throws for unknown message type', () => {
		const packet = new TaskAckownledgedPacket();
		packet.matchId = '123456789023';
		const encoded = encodePacket(packet);
		const bytes = new Uint8Array(encoded);

		expect(bytes[0] | (bytes[1] << 8)).toBe(PROTOCOL_MAGIC);
		bytes[3] = 255;

		expect(() => decodePacket(bytes.buffer)).toThrowError('Unknown message type');
	});

	test('throws for invalid snowflake id format', () => {
		const packet = new TaskAckownledgedPacket();
		packet.matchId = '42';
		packet.canAssignMore = true;

		expect(() => encodePacket(packet)).toThrowError('TaskAckownledgedPacket.matchId must be a 12-digit snowflake ID string');
	});

	test('maps winner index correctly', () => {
		expect(winnerIndexOf(MatchResult.P1Win)).toBe(1);
		expect(winnerIndexOf(MatchResult.P2Win)).toBe(2);
		expect(winnerIndexOf(MatchResult.Tied)).toBeNull();
		expect(winnerIndexOf(MatchResult.NoResult)).toBeNull();
	});

	test('message type enum values stay stable', () => {
		expect(MessageType.Hello).toBe(1);
		expect(MessageType.TaskAssign).toBe(4);
		expect(MessageType.TaskResult).toBe(5);
		expect(MessageType.TaskAckownledged).toBe(6);
	});
});
