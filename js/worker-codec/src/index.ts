import { BASIC_TYPES, CompoundTypeHandler } from 'structpack';

export enum MessageType {
	Hello = 1,
	TaskAssign = 4,
	TaskResult = 5,
	TaskAckownledged = 6,
}

export const CODEC_VERSION = 1;
export const PROTOCOL_MAGIC = 0x4354;
export const SNOWFLAKE_ID_LENGTH = 12;

export type SnowflakeId = string;

export function isSnowflakeId(value: unknown): value is SnowflakeId {
	return typeof value === 'string' && value.length === SNOWFLAKE_ID_LENGTH;
}

function assertSnowflakeId(value: unknown, fieldName: string): asserts value is SnowflakeId {
	if (!isSnowflakeId(value)) {
		throw new Error(`${fieldName} must be a ${SNOWFLAKE_ID_LENGTH}-digit snowflake ID string`);
	}
}

export class PacketHeader {
	[key: string]: unknown;

	protocolMagic!: number;
	codecVersion!: number;
	messageType!: MessageType;
	packetLength!: number; // Length of the payload in bytes, not including the header

	static typedef: Array<{ field: string; type: any }> = [
		{ field: 'protocolMagic', type: BASIC_TYPES.u16 },
		{ field: 'codecVersion', type: BASIC_TYPES.u8 },
		{ field: 'messageType', type: BASIC_TYPES.u8 },
		{ field: 'packetLength', type: BASIC_TYPES.u32 },
	];
}

export class StrategyGroupDescriptor {
	[key: string]: unknown;

	strategyGroupId!: SnowflakeId;
	baseLastModified!: Date;
	unitLastModified!: Date;
	baseStrategyUrl!: string;
	unitStrategyUrl!: string;

	static typedef: Array<{ field: string; type: any }> = [
		{ field: 'strategyGroupId', type: BASIC_TYPES.str },
		{ field: 'baseLastModified', type: BASIC_TYPES.DateTime },
		{ field: 'unitLastModified', type: BASIC_TYPES.DateTime },
		{ field: 'baseStrategyUrl', type: BASIC_TYPES.str },
		{ field: 'unitStrategyUrl', type: BASIC_TYPES.str },
	];
}

export class TaskAssignPacket {
	[key: string]: unknown;

	matchId!: SnowflakeId;
	mapData!: ArrayBuffer;
	strategies!: [StrategyGroupDescriptor, StrategyGroupDescriptor];
	replayUploadUrl!: string;

	static typedef: Array<{ field: string; type: any }> = [
		{ field: 'matchId', type: BASIC_TYPES.str },
		{ field: 'mapData', type: BASIC_TYPES.arrayBuffer },
		{ field: 'strategies', type: BASIC_TYPES.FixedArray(2, StrategyGroupDescriptor) },
		{ field: 'replayUploadUrl', type: BASIC_TYPES.str },
	];
}

export class TaskAckownledgedPacket {
	[key: string]: unknown;

	matchId!: SnowflakeId;
	canAssignMore!: boolean;

	static typedef: Array<{ field: string; type: any }> = [
		{ field: 'matchId', type: BASIC_TYPES.str },
		{ field: 'canAssignMore', type: BASIC_TYPES.bool },
	];
}

export enum MatchResult {
	P1Win = 1,
	P2Win = 2,
	Tied = 3,
	NoResult = 4,
}

export function winnerIndexOf(result: MatchResult): number | null {
	switch (result) {
		case MatchResult.P1Win:
			return 1;
		case MatchResult.P2Win:
			return 2;
		case MatchResult.Tied:
		case MatchResult.NoResult:
			return null;
	}
}

export enum TaskStatus {
	Success = 1,
	CoreCrash = 2,
	Polluted = 3,
}

export class StrategyExecutionCrashInfo {
	[key: string]: unknown;

	strategyGroupId!: SnowflakeId;
	baseStrategyCrashed!: boolean;
	unitStrategyCrashed!: boolean;

	static typedef: Array<{ field: string; type: any }> = [
		{ field: 'strategyGroupId', type: BASIC_TYPES.str },
		{ field: 'baseStrategyCrashed', type: BASIC_TYPES.bool },
		{ field: 'unitStrategyCrashed', type: BASIC_TYPES.bool },
	];
}

export class TaskResultPacket {
	[key: string]: unknown;

	matchId!: SnowflakeId;
	result!: MatchResult;
	status!: TaskStatus;
	errorLog!: string;
	crashInfo!: [StrategyExecutionCrashInfo, StrategyExecutionCrashInfo];
	finishedAt!: Date;

	static typedef: Array<{ field: string; type: any }> = [
		{ field: 'matchId', type: BASIC_TYPES.str },
		{ field: 'result', type: BASIC_TYPES.u8 },
		{ field: 'status', type: BASIC_TYPES.u8 },
		{ field: 'errorLog', type: BASIC_TYPES.str },
		{ field: 'crashInfo', type: BASIC_TYPES.FixedArray(2, StrategyExecutionCrashInfo) },
		{ field: 'finishedAt', type: BASIC_TYPES.DateTime },
	];
}

export class HelloPacket {
	[key: string]: unknown;

	static typedef = [];
}

export type Packet = TaskAssignPacket | TaskAckownledgedPacket | TaskResultPacket | HelloPacket;

function validatePacketIds(packet: Packet): void {
	if (packet instanceof HelloPacket) {
		return;
	}

	if (packet instanceof TaskAssignPacket) {
		assertSnowflakeId(packet.matchId, 'TaskAssignPacket.matchId');
		assertSnowflakeId(packet.strategies[0].strategyGroupId, 'TaskAssignPacket.strategies[0].strategyGroupId');
		assertSnowflakeId(packet.strategies[1].strategyGroupId, 'TaskAssignPacket.strategies[1].strategyGroupId');
		return;
	}

	if (packet instanceof TaskAckownledgedPacket) {
		assertSnowflakeId(packet.matchId, 'TaskAckownledgedPacket.matchId');
		return;
	}

	if (packet instanceof TaskResultPacket) {
		assertSnowflakeId(packet.matchId, 'TaskResultPacket.matchId');
		assertSnowflakeId(packet.crashInfo[0].strategyGroupId, 'TaskResultPacket.crashInfo[0].strategyGroupId');
		assertSnowflakeId(packet.crashInfo[1].strategyGroupId, 'TaskResultPacket.crashInfo[1].strategyGroupId');
	}
}

export function getPacketType(packet: Packet): MessageType {
	if (packet instanceof TaskAssignPacket) {
		return MessageType.TaskAssign;
	} else if (packet instanceof TaskAckownledgedPacket) {
		return MessageType.TaskAckownledged;
	} else if (packet instanceof TaskResultPacket) {
		return MessageType.TaskResult;
	} else if (packet instanceof HelloPacket) {
		return MessageType.Hello;
	} else {
		throw new Error('Unknown packet type');
	}
}

const headerHandler = new CompoundTypeHandler(PacketHeader);
const taskAssignHandler = new CompoundTypeHandler(TaskAssignPacket);
const taskAckHandler = new CompoundTypeHandler(TaskAckownledgedPacket);
const taskResultHandler = new CompoundTypeHandler(TaskResultPacket);
const helloHandler = new CompoundTypeHandler(HelloPacket);

export function getPacketHandler(messageType: MessageType): CompoundTypeHandler {
	switch (messageType) {
		case MessageType.TaskAssign:
			return taskAssignHandler;
		case MessageType.TaskAckownledged:
			return taskAckHandler;
		case MessageType.TaskResult:
			return taskResultHandler;
		case MessageType.Hello:
			return helloHandler;
		default:
			throw new Error('Unknown message type');
	}
}

export function encodePacket(packet: Packet): ArrayBuffer {
	validatePacketIds(packet);

	const header = new PacketHeader();
	header.protocolMagic = PROTOCOL_MAGIC;
	header.codecVersion = CODEC_VERSION;
	header.messageType = getPacketType(packet);

	const handler = getPacketHandler(header.messageType);
	header.packetLength = handler.sizeof(packet);

	const buffer = new ArrayBuffer(header.packetLength + headerHandler.sizeof(header));
	const view = new DataView(buffer);
	let offset = headerHandler.serialize(view, 0, header);
	handler.serialize(view, offset, packet);

	return buffer;
}

export function decodePacket(buffer: ArrayBuffer): Packet {
	const view = new DataView(buffer);
	const headerRes = headerHandler.deserialize(view, 0);
	const header = headerRes.value as PacketHeader;
	if (header.protocolMagic !== PROTOCOL_MAGIC) {
		throw new Error('Invalid protocol magic');
	}

	if (header.codecVersion !== CODEC_VERSION) {
		throw new Error('Unsupported codec version');
	}

	// Validate that the buffer contains the full packet
	// Ignore extra data after the packet, for error tolerance
	if (buffer.byteLength < headerRes.offset + header.packetLength) {
		throw new Error('Buffer does not contain full packet');
	}

	const handler = getPacketHandler(header.messageType);
	const packetRes = handler.deserialize(view, headerRes.offset);
	const packet = packetRes.value as Packet;
	validatePacketIds(packet);
	return packet;
}
