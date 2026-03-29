import process from 'node:process';
import { setTimeout as sleep } from 'node:timers/promises';
import WebSocket from 'ws';
import {
	decodePacket,
	encodePacket,
	CODEC_VERSION,
	PROTOCOL_MAGIC,
	TaskAckownledgedPacket,
	TaskAssignPacket,
	type TaskResultPacket,
} from '@corertt/worker-codec';
import { ElfCache } from './cache.js';
import { compressZstd } from './compression.js';
import { runHeadless } from './headless.js';
import { buildTaskCoreCrashResult, buildTaskSuccessResult } from './result.js';
import type { AssignedTask, RunningTask, WorkerConfig, WorkerEvents } from './types.js';

interface WorkerRuntimeOptions {
	stopAfterResults?: number;
}

function encodeHelloPacket(): Buffer {
	const buffer = new ArrayBuffer(8);
	const view = new DataView(buffer);
	view.setUint16(0, PROTOCOL_MAGIC, true);
	view.setUint8(2, CODEC_VERSION);
	view.setUint8(3, 1);
	view.setUint32(4, 0, true);
	return Buffer.from(buffer);
}

function asArrayBuffer(data: WebSocket.RawData): ArrayBuffer {
	if (data instanceof ArrayBuffer) {
		return data;
	}
	if (Array.isArray(data)) {
		const merged = Buffer.concat(data.map((item) => Buffer.from(item)));
		return merged.buffer.slice(merged.byteOffset, merged.byteOffset + merged.byteLength);
	}
	const buffer = Buffer.from(data);
	return buffer.buffer.slice(buffer.byteOffset, buffer.byteOffset + buffer.byteLength);
}

function buildEndpoint(baseUrl: string, endpointPath: string, websocket = false): string {
	const normalized = baseUrl.endsWith('/') ? baseUrl : `${baseUrl}/`;
	const url = new URL(endpointPath, normalized);
	if (websocket) {
		url.protocol = url.protocol === 'https:' ? 'wss:' : 'ws:';
	}
	return url.toString();
}

export class CoreWorker {
	private readonly config: WorkerConfig;
	private readonly events: WorkerEvents;
	private readonly runtimeOptions: WorkerRuntimeOptions;
	private readonly cache: ElfCache;
	private readonly queue: AssignedTask[] = [];
	private readonly running = new Map<number, RunningTask>();
	private readonly pendingResults: TaskResultPacket[] = [];
	private stopped = false;
	private loopPromise: Promise<void> | null = null;
	private ws: WebSocket | null = null;
	private token = '';
	private completedResults = 0;
	private idleAvailabilitySent = false;

	public constructor(config: WorkerConfig, events: WorkerEvents = {}, runtimeOptions: WorkerRuntimeOptions = {}) {
		this.config = config;
		this.events = events;
		this.runtimeOptions = runtimeOptions;
		this.cache = new ElfCache({
			cacheDir: config.elfCacheDir,
			cacheLimitBytes: config.elfCacheLimitBytes,
		});
	}

	public async start(): Promise<void> {
		if (this.loopPromise != null) {
			return;
		}
		this.stopped = false;
		await this.cache.initialize();
		this.loopPromise = this.runConnectionLoop();
	}

	public async stop(): Promise<void> {
		this.stopped = true;
		if (this.ws != null) {
			this.ws.close();
		}
		if (this.loopPromise != null) {
			await this.loopPromise;
		}
		this.loopPromise = null;
	}

	private async runConnectionLoop(): Promise<void> {
		let retryDelayMs = 1000;
		while (!this.stopped) {
			try {
				this.token = await this.authenticate();
				this.cache.setBearerToken(this.token);
				await this.connectAndServe(this.token);
				retryDelayMs = 1000;
			} catch (error) {
				const message = error instanceof Error ? error.message : String(error);
				this.events.onRuntimeError?.(message);
				process.stderr.write(`worker reconnect: ${message}\n`);
				if (this.stopped) {
					break;
				}
				await sleep(retryDelayMs);
				retryDelayMs = Math.min(30000, retryDelayMs * 2);
			}
		}
	}

	private async authenticate(): Promise<string> {
		const url = buildEndpoint(this.config.backendUrl, 'api/worker/authenticate', false);
		const response = await fetch(url, {
			method: 'POST',
			headers: {
				'Content-Type': 'application/json',
			},
			body: JSON.stringify({
				worker_id: this.config.workerId,
				secret: this.config.workerSecret,
			}),
		});
		if (!response.ok) {
			throw new Error(`authentication failed: ${response.status} ${response.statusText}`);
		}
		const data = await response.json() as { token?: string };
		if (typeof data.token !== 'string' || data.token.length === 0) {
			throw new Error('authentication response missing token');
		}
		return data.token;
	}

	private async connectAndServe(token: string): Promise<void> {
		const url = buildEndpoint(this.config.backendUrl, 'api/worker/communication', true);
		const ws = new WebSocket(url, {
			headers: {
				Authorization: `Bearer ${token}`,
			},
		});
		this.ws = ws;
		ws.on('message', (data) => {
			void this.handleIncomingMessage(data);
		});

		await new Promise<void>((resolve, reject) => {
			ws.once('open', () => resolve());
			ws.once('error', reject);
		});

		this.sendHello();
		this.replayUnfinishedAcknowledgements();
		this.flushPendingResults();
		this.maybeSendIdleAvailability();

		await new Promise<void>((resolve, reject) => {
			ws.once('close', () => resolve());
			ws.once('error', (error) => reject(error));
		});

		if (this.ws === ws) {
			this.ws = null;
		}
	}

	private sendPacket(packet: TaskAckownledgedPacket | TaskResultPacket): void {
		if (this.ws == null || this.ws.readyState !== WebSocket.OPEN) {
			return;
		}
		const encoded = encodePacket(packet);
		this.ws.send(Buffer.from(encoded));
	}

	private sendHello(): void {
		if (this.ws == null || this.ws.readyState !== WebSocket.OPEN) {
			return;
		}
		this.ws.send(encodeHelloPacket());
	}

	private computeCanAssignMore(): boolean {
		return this.running.size + this.queue.length < this.config.concurrency;
	}

	private sendTaskAck(matchId: number): void {
		const packet = new TaskAckownledgedPacket();
		packet.matchId = matchId;
		packet.canAssignMore = this.computeCanAssignMore();
		this.sendPacket(packet);
	}

	private replayUnfinishedAcknowledgements(): void {
		for (const task of this.queue) {
			this.sendTaskAck(task.packet.matchId);
		}
		for (const runningTask of this.running.values()) {
			this.sendTaskAck(runningTask.task.packet.matchId);
		}
	}

	private maybeSendIdleAvailability(): void {
		if (this.idleAvailabilitySent) {
			return;
		}
		if (this.running.size > 0 || this.queue.length > 0) {
			return;
		}
		const packet = new TaskAckownledgedPacket();
		packet.matchId = 0;
		packet.canAssignMore = true;
		this.sendPacket(packet);
		this.idleAvailabilitySent = true;
	}

	private async handleIncomingMessage(rawData: WebSocket.RawData): Promise<void> {
		let packet;
		try {
			packet = decodePacket(asArrayBuffer(rawData));
		} catch (error) {
			const message = error instanceof Error ? error.message : String(error);
			this.events.onDecodeError?.(message);
			process.stderr.write(`worker decode error: ${message}\n`);
			if (this.ws != null) {
				this.ws.close();
			}
			return;
		}

		if (!(packet instanceof TaskAssignPacket)) {
			return;
		}

		this.events.onTaskAssigned?.(packet);
		this.idleAvailabilitySent = false;
		const task: AssignedTask = {
			packet,
			enqueuedAtMs: Date.now(),
		};
		this.queue.push(task);
		this.sendTaskAck(packet.matchId);
		this.maybeStartTasks();
	}

	private maybeStartTasks(): void {
		while (this.running.size < this.config.concurrency && this.queue.length > 0) {
			const task = this.queue.shift();
			if (task == null) {
				break;
			}
			const runningTask: RunningTask = {
				task,
				startedAtMs: Date.now(),
			};
			this.running.set(task.packet.matchId, runningTask);
			void this.executeTask(runningTask)
				.finally(() => {
					this.running.delete(task.packet.matchId);
					this.maybeStartTasks();
					this.maybeSendIdleAvailability();
				});
		}
	}

	private async executeTask(runningTask: RunningTask): Promise<void> {
		const packet = runningTask.task.packet;
		let resultPacket: TaskResultPacket;
		let stderrCombined = '';
		try {
			const p1Descriptor = packet.strategies[0];
			const p2Descriptor = packet.strategies[1];
			const strategyPaths = {
				p1BasePath: await this.cache.getOrDownload(p1Descriptor, 'base'),
				p1UnitPath: await this.cache.getOrDownload(p1Descriptor, 'unit'),
				p2BasePath: await this.cache.getOrDownload(p2Descriptor, 'base'),
				p2UnitPath: await this.cache.getOrDownload(p2Descriptor, 'unit'),
			};

			const runResult = await runHeadless(this.config, {
				mapData: packet.mapData,
				strategies: strategyPaths,
			});
			stderrCombined = runResult.stderr;
			if (runResult.timedOut) {
				stderrCombined += '\nheadless timeout reached';
				resultPacket = buildTaskCoreCrashResult(packet.matchId, packet.strategies, stderrCombined, this.config.errorLogMaxBytes);
				await this.dispatchTaskResult(resultPacket);
				return;
			}
			if (runResult.exitSignal != null) {
				stderrCombined += `\nheadless terminated by signal ${runResult.exitSignal}`;
				resultPacket = buildTaskCoreCrashResult(packet.matchId, packet.strategies, stderrCombined, this.config.errorLogMaxBytes);
				await this.dispatchTaskResult(resultPacket);
				return;
			}
			if (runResult.exitCode !== 0) {
				stderrCombined += `\nheadless exited with code ${String(runResult.exitCode)}`;
				resultPacket = buildTaskCoreCrashResult(packet.matchId, packet.strategies, stderrCombined, this.config.errorLogMaxBytes);
				await this.dispatchTaskResult(resultPacket);
				return;
			}

			let compressedReplay: Buffer;
			try {
				compressedReplay = await compressZstd(runResult.stdout);
			} catch (error) {
				const message = error instanceof Error ? error.message : String(error);
				stderrCombined += `\nfailed to compress replay: ${message}`;
				resultPacket = buildTaskCoreCrashResult(packet.matchId, packet.strategies, stderrCombined, this.config.errorLogMaxBytes);
				await this.dispatchTaskResult(resultPacket);
				return;
			}

			try {
				await this.uploadReplay(packet.replayUploadUrl, compressedReplay);
			} catch (error) {
				const message = error instanceof Error ? error.message : String(error);
				stderrCombined += `\nfailed to upload replay: ${message}`;
				resultPacket = buildTaskCoreCrashResult(packet.matchId, packet.strategies, stderrCombined, this.config.errorLogMaxBytes);
				await this.dispatchTaskResult(resultPacket);
				return;
			}

			try {
				resultPacket = buildTaskSuccessResult(packet.matchId, runResult.stdout, packet.strategies, stderrCombined, this.config.errorLogMaxBytes);
			} catch (error) {
				const message = error instanceof Error ? error.message : String(error);
				stderrCombined += `\nfailed to decode replay: ${message}`;
				resultPacket = buildTaskCoreCrashResult(packet.matchId, packet.strategies, stderrCombined, this.config.errorLogMaxBytes);
			}
		} catch (error) {
			const message = error instanceof Error ? error.message : String(error);
			stderrCombined += `\nworker execution failure: ${message}`;
			resultPacket = buildTaskCoreCrashResult(packet.matchId, packet.strategies, stderrCombined, this.config.errorLogMaxBytes);
		}

		await this.dispatchTaskResult(resultPacket);
	}

	private async uploadReplay(url: string, body: Buffer): Promise<void> {
		const payload = new Uint8Array(body);
		const response = await fetch(url, {
			method: 'PUT',
			headers: {
				'Content-Type': 'application/zstd',
			},
			body: payload,
		});
		if (!response.ok) {
			throw new Error(`${response.status} ${response.statusText}`);
		}
	}

	private async dispatchTaskResult(packet: TaskResultPacket): Promise<void> {
		this.events.onTaskResult?.(packet);
		if (this.ws == null || this.ws.readyState !== WebSocket.OPEN) {
			this.pendingResults.push(packet);
		} else {
			this.sendPacket(packet);
		}
		this.completedResults += 1;
		if (this.runtimeOptions.stopAfterResults != null && this.completedResults >= this.runtimeOptions.stopAfterResults) {
			await this.stop();
		}
	}

	private flushPendingResults(): void {
		if (this.ws == null || this.ws.readyState !== WebSocket.OPEN) {
			return;
		}
		while (this.pendingResults.length > 0) {
			const packet = this.pendingResults.shift();
			if (packet != null) {
				this.sendPacket(packet);
			}
		}
	}
}
