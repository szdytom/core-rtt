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
	strategyGroupId: number,
	baseStrategyUrl: string,
	unitStrategyUrl: string,
): StrategyGroupDescriptor {
	const descriptor = new StrategyGroupDescriptor();
	descriptor.strategyGroupId = strategyGroupId;
	descriptor.baseStrategyUrl = baseStrategyUrl;
	descriptor.unitStrategyUrl = unitStrategyUrl;
	return descriptor;
}

function createCrashInfo(
	strategyGroupId: number,
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
		packet.matchId = 42;
		packet.mapData = new Uint8Array([1, 2, 3, 4]).buffer;
		packet.strategies = [
			createStrategyGroupDescriptor(11, 'https://example.com/base/a.ts', 'https://example.com/unit/a.ts'),
			createStrategyGroupDescriptor(22, 'https://example.com/base/b.ts', 'https://example.com/unit/b.ts'),
		];
		packet.replayUploadUrl = 'https://example.com/replays/42';

		const encoded = encodePacket(packet);
		const decoded = decodePacket(encoded);

		expect(decoded).toBeInstanceOf(TaskAssignPacket);
		const decodedAssign = decoded as TaskAssignPacket;
		expect(decodedAssign.matchId).toBe(42);
		expect(Array.from(new Uint8Array(decodedAssign.mapData))).toEqual([1, 2, 3, 4]);
		expect(decodedAssign.strategies[0].strategyGroupId).toBe(11);
		expect(decodedAssign.strategies[1].strategyGroupId).toBe(22);
		expect(decodedAssign.replayUploadUrl).toBe('https://example.com/replays/42');
	});

	test('round-trips TaskResult packets including errorLog', () => {
		const packet = new TaskResultPacket();
		packet.matchId = 9;
		packet.result = MatchResult.P2Win;
		packet.status = TaskStatus.CoreCrash;
		packet.errorLog = 'segfault in core';
		packet.crashInfo = [
			createCrashInfo(1, true, false),
			createCrashInfo(2, false, true),
		];
		packet.finishedAt = new Date('2026-03-01T12:30:00.000Z');

		const encoded = encodePacket(packet);
		const decoded = decodePacket(encoded);

		expect(decoded).toBeInstanceOf(TaskResultPacket);
		const decodedResult = decoded as TaskResultPacket;
		expect(decodedResult.matchId).toBe(9);
		expect(decodedResult.result).toBe(MatchResult.P2Win);
		expect(decodedResult.status).toBe(TaskStatus.CoreCrash);
		expect(decodedResult.errorLog).toBe('segfault in core');
		expect(decodedResult.crashInfo[0].baseStrategyCrashed).toBe(true);
		expect(decodedResult.crashInfo[1].unitStrategyCrashed).toBe(true);
		expect(decodedResult.finishedAt.getTime()).toBe(packet.finishedAt.getTime());
	});

	test('throws for invalid protocol magic', () => {
		const packet = new TaskAckownledgedPacket();
		packet.matchId = 1;
		const encoded = encodePacket(packet);
		const bytes = new Uint8Array(encoded);

		bytes[0] = 0;
		bytes[1] = 0;
		expect(() => decodePacket(bytes.buffer)).toThrowError('Invalid protocol magic');
	});

	test('throws for unsupported codec version', () => {
		const packet = new TaskAckownledgedPacket();
		packet.matchId = 2;
		const encoded = encodePacket(packet);
		const bytes = new Uint8Array(encoded);

		bytes[2] = 2;
		expect(() => decodePacket(bytes.buffer)).toThrowError('Unsupported codec version');
	});

	test('throws when packet body is truncated', () => {
		const packet = new TaskAssignPacket();
		packet.matchId = 7;
		packet.mapData = new Uint8Array([7, 7, 7]).buffer;
		packet.strategies = [
			createStrategyGroupDescriptor(1, 'a', 'b'),
			createStrategyGroupDescriptor(2, 'c', 'd'),
		];
		packet.replayUploadUrl = 'https://example.com/upload';
		const encoded = encodePacket(packet);

		const truncated = encoded.slice(0, encoded.byteLength - 2);
		expect(() => decodePacket(truncated)).toThrowError('Buffer does not contain full packet');
	});

	test('throws for unknown message type', () => {
		const packet = new TaskAckownledgedPacket();
		packet.matchId = 3;
		const encoded = encodePacket(packet);
		const bytes = new Uint8Array(encoded);

		expect(bytes[0] | (bytes[1] << 8)).toBe(PROTOCOL_MAGIC);
		bytes[3] = 255;

		expect(() => decodePacket(bytes.buffer)).toThrowError('Unknown message type');
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
