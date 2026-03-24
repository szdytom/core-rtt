export type ReplayInput = ArrayBuffer | Uint8Array;

export interface ReplayTile {
	terrain: number;
	side: number;
	isResource: boolean;
	isBase: boolean;
}

export interface ReplayTilemap {
	width: number;
	height: number;
	baseSize: number;
	tiles: ReplayTile[];
}

export interface ReplayHeader {
	magic: 'CRPL';
	version: 4;
	headerSize: number;
	tilemap: ReplayTilemap;
}

export interface ReplayPlayer {
	id: number;
	baseEnergy: number;
	baseCaptureCounter: number;
}

export interface ReplayUnit {
	id: number;
	playerId: number;
	x: number;
	y: number;
	health: number;
	energy: number;
	attackCooldown: number;
	upgrades: number;
}

export interface ReplayBullet {
	x: number;
	y: number;
	direction: number;
	playerId: number;
	damage: number;
}

export type ReplayLogSource = 0 | 1;

export type ReplayLogType = 0 | 1 | 2 | 3 | 4;

export interface ReplayLogEntry {
	tick: number;
	playerId: number;
	unitId: number;
	source: ReplayLogSource;
	type: ReplayLogType;
	payload: Uint8Array;
}

export interface ReplayTickFrame {
	tick: number;
	players: [ReplayPlayer, ReplayPlayer];
	units: ReplayUnit[];
	bullets: ReplayBullet[];
	logs: ReplayLogEntry[];
}

export type ReplayTermination = 'completed' | 'aborted';

export interface ReplayEndMarker {
	termination: ReplayTermination;
	winnerPlayerId: 0 | 1 | 2;
}

export interface ReplayData {
	header: ReplayHeader;
	ticks: ReplayTickFrame[];
	endMarker: ReplayEndMarker;
}
