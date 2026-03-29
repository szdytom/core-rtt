import { MatchResult, StrategyExecutionCrashInfo, TaskResultPacket, TaskStatus } from '@corertt/worker-codec';
import { decodeReplay as decodeReplayFile } from '@corertt/corereplay';
import type { StrategyGroupDescriptor } from '@corertt/worker-codec';

function truncateUtf8(input: string, maxBytes: number): string {
	const encoded = Buffer.from(input, 'utf8');
	if (encoded.byteLength <= maxBytes) {
		return input;
	}
	const sliced = encoded.subarray(0, maxBytes);
	return sliced.toString('utf8');
}

function buildCrashInfo(strategies: [StrategyGroupDescriptor, StrategyGroupDescriptor]): [StrategyExecutionCrashInfo, StrategyExecutionCrashInfo] {
	const p1 = new StrategyExecutionCrashInfo();
	p1.strategyGroupId = strategies[0].strategyGroupId;
	p1.baseStrategyCrashed = false;
	p1.unitStrategyCrashed = false;
	const p2 = new StrategyExecutionCrashInfo();
	p2.strategyGroupId = strategies[1].strategyGroupId;
	p2.baseStrategyCrashed = false;
	p2.unitStrategyCrashed = false;
	return [p1, p2];
}

function matchResultFromWinner(winner: 0 | 1 | 2): MatchResult {
	if (winner === 1) {
		return MatchResult.P1Win;
	}
	if (winner === 2) {
		return MatchResult.P2Win;
	}
	return MatchResult.Tied;
}

export function buildTaskSuccessResult(matchId: number, replayStdout: Buffer, strategies: [StrategyGroupDescriptor, StrategyGroupDescriptor], stderrText: string, errorLogMaxBytes: number): TaskResultPacket {
	const replay = decodeReplayFile(replayStdout);
	const packet = new TaskResultPacket();
	packet.matchId = matchId;
	packet.result = matchResultFromWinner(replay.endMarker.winnerPlayerId);
	packet.status = TaskStatus.Success;
	packet.errorLog = truncateUtf8(stderrText, errorLogMaxBytes);
	packet.crashInfo = buildCrashInfo(strategies);
	packet.finishedAt = new Date();
	return packet;
}

export function buildTaskCoreCrashResult(matchId: number, strategies: [StrategyGroupDescriptor, StrategyGroupDescriptor], stderrText: string, errorLogMaxBytes: number): TaskResultPacket {
	const packet = new TaskResultPacket();
	packet.matchId = matchId;
	packet.result = MatchResult.NoResult;
	packet.status = TaskStatus.CoreCrash;
	packet.errorLog = truncateUtf8(stderrText, errorLogMaxBytes);
	packet.crashInfo = buildCrashInfo(strategies);
	packet.finishedAt = new Date();
	return packet;
}
