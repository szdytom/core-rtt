import type { TaskAssignPacket, TaskResultPacket } from '@corertt/worker-codec';

export interface WorkerConfig {
	repoRoot: string;
	backendUrl: string;
	workerId: string;
	workerSecret: string;
	headlessPath: string;
	timeoutSeconds: number;
	maxTicks: number;
	elfCacheDir: string;
	elfCacheLimitBytes: number;
	tmpMapDir: string;
	concurrency: number;
	errorLogMaxBytes: number;
}

export interface StrategyBinaryRefs {
	p1BasePath: string;
	p1UnitPath: string;
	p2BasePath: string;
	p2UnitPath: string;
}

export interface HeadlessRunResult {
	stdout: Buffer;
	stderr: string;
	timedOut: boolean;
	exitCode: number | null;
	exitSignal: NodeJS.Signals | null;
}

export interface AssignedTask {
	packet: TaskAssignPacket;
	enqueuedAtMs: number;
}

export interface RunningTask {
	task: AssignedTask;
	startedAtMs: number;
}

export interface WorkerEvents {
	onTaskResult?: (result: TaskResultPacket) => void;
	onTaskAssigned?: (task: TaskAssignPacket) => void;
	onDecodeError?: (message: string) => void;
	onRuntimeError?: (message: string) => void;
}
