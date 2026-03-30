import { MatchResult, StrategyExecutionCrashInfo, TaskResultPacket, TaskStatus } from '@corertt/worker-codec';
import type { StrategyGroupDescriptor } from '@corertt/worker-codec';

interface WorkerTerminationPayload {
	termination: 'completed' | 'aborted';
	winnerPlayerId: 0 | 1 | 2;
	p1BaseCrash: boolean;
	p1UnitCrash: boolean;
	p2BaseCrash: boolean;
	p2UnitCrash: boolean;
}

function truncateUtf8(input: string, maxBytes: number): string {
	const encoded = Buffer.from(input, 'utf8');
	if (encoded.byteLength <= maxBytes) {
		return input;
	}
	const sliced = encoded.subarray(0, maxBytes);
	return sliced.toString('utf8');
}

function parseBooleanField(value: unknown): boolean | null {
	if (typeof value === 'boolean') {
		return value;
	}
	return null;
}

function parseWinnerPlayerId(value: unknown): 0 | 1 | 2 | null {
	if (value === 0 || value === 1 || value === 2) {
		return value;
	}
	return null;
}

function parseTerminationPayload(line: string): WorkerTerminationPayload | null {
	let parsed: unknown;
	try {
		parsed = JSON.parse(line);
	} catch {
		return null;
	}
	if (parsed == null || typeof parsed !== 'object') {
		return null;
	}
	const record = parsed as Record<string, unknown>;
	if (record.termination !== 'completed' && record.termination !== 'aborted') {
		return null;
	}
	const winnerPlayerId = parseWinnerPlayerId(record.winner_player_id);
	const p1BaseCrash = parseBooleanField(record.p1_base_crash);
	const p1UnitCrash = parseBooleanField(record.p1_unit_crash);
	const p2BaseCrash = parseBooleanField(record.p2_base_crash);
	const p2UnitCrash = parseBooleanField(record.p2_unit_crash);
	if (
		winnerPlayerId == null || p1BaseCrash == null || p1UnitCrash == null
		|| p2BaseCrash == null || p2UnitCrash == null
	) {
		return null;
	}
	return {
		termination: record.termination,
		winnerPlayerId,
		p1BaseCrash,
		p1UnitCrash,
		p2BaseCrash,
		p2UnitCrash,
	};
}

function parseLastTerminationPayload(stderrText: string): WorkerTerminationPayload | null {
	let lastPayload: WorkerTerminationPayload | null = null;
	for (const line of stderrText.split(/\r?\n/)) {
		const trimmed = line.trim();
		if (trimmed.length === 0) {
			continue;
		}
		const payload = parseTerminationPayload(trimmed);
		if (payload != null) {
			lastPayload = payload;
		}
	}
	return lastPayload;
}

function buildCrashInfo(
	strategies: [StrategyGroupDescriptor, StrategyGroupDescriptor],
	payload: WorkerTerminationPayload | null,
): [StrategyExecutionCrashInfo, StrategyExecutionCrashInfo] {
	const p1 = new StrategyExecutionCrashInfo();
	p1.strategyGroupId = strategies[0].strategyGroupId;
	p1.baseStrategyCrashed = payload?.p1BaseCrash ?? false;
	p1.unitStrategyCrashed = payload?.p1UnitCrash ?? false;
	const p2 = new StrategyExecutionCrashInfo();
	p2.strategyGroupId = strategies[1].strategyGroupId;
	p2.baseStrategyCrashed = payload?.p2BaseCrash ?? false;
	p2.unitStrategyCrashed = payload?.p2UnitCrash ?? false;
	return [p1, p2];
}

function matchResultFromPayload(payload: WorkerTerminationPayload): MatchResult {
	if (payload.termination === 'aborted') {
		return MatchResult.Tied;
	}
	if (payload.winnerPlayerId === 1) {
		return MatchResult.P1Win;
	}
	if (payload.winnerPlayerId === 2) {
		return MatchResult.P2Win;
	}
	throw new Error('invalid worker termination payload: completed result must have winner_player_id 1 or 2');
}

export function buildTaskSuccessResult(matchId: number, strategies: [StrategyGroupDescriptor, StrategyGroupDescriptor], stderrText: string, errorLogMaxBytes: number): TaskResultPacket {
	const payload = parseLastTerminationPayload(stderrText);
	if (payload == null) {
		throw new Error('missing worker termination jsonl marker in stderr');
	}
	const packet = new TaskResultPacket();
	packet.matchId = matchId;
	packet.result = matchResultFromPayload(payload);
	packet.status = TaskStatus.Success;
	packet.errorLog = truncateUtf8(stderrText, errorLogMaxBytes);
	packet.crashInfo = buildCrashInfo(strategies, payload);
	packet.finishedAt = new Date();
	return packet;
}

export function buildTaskCoreCrashResult(matchId: number, strategies: [StrategyGroupDescriptor, StrategyGroupDescriptor], stderrText: string, errorLogMaxBytes: number): TaskResultPacket {
	const payload = parseLastTerminationPayload(stderrText);
	const packet = new TaskResultPacket();
	packet.matchId = matchId;
	packet.result = MatchResult.NoResult;
	packet.status = TaskStatus.CoreCrash;
	packet.errorLog = truncateUtf8(stderrText, errorLogMaxBytes);
	packet.crashInfo = buildCrashInfo(strategies, payload);
	packet.finishedAt = new Date();
	return packet;
}
