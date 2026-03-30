import WebSocket from 'ws';
import {
	decodePacket,
	encodePacket,
	HelloPacket,
	Packet,
	TaskAckownledgedPacket,
	TaskAssignPacket,
	type TaskResultPacket,
} from '@corertt/worker-codec';
import { ElfCache } from './cache.js';
import { runHeadless } from './headless.js';
import { logger } from './logger.js';
import { buildTaskCoreCrashResult, buildTaskSuccessResult } from './result.js';
import { TaskScheduler } from './scheduler.js';
import type { ScheduledTask } from './scheduler.js';
import type { CoreWorkerEventDetailMap, WorkerConfig } from './types.js';

interface WorkerRuntimeOptions {
	stopAfterResults?: number;
}

const workerLogger = logger.child({ component: 'worker' });

function createAbortError(signal: AbortSignal): unknown {
	return signal.reason ?? new DOMException('The operation was aborted.', 'AbortError');
}

async function sleepWithSignal(ms: number, signal: AbortSignal): Promise<void> {
	signal.throwIfAborted();
	await new Promise<void>((resolve, reject) => {
		const timer = setTimeout(() => {
			signal.removeEventListener('abort', onAbort);
			resolve();
		}, ms);

		const onAbort = () => {
			clearTimeout(timer);
			reject(createAbortError(signal));
		};

		signal.addEventListener('abort', onAbort, { once: true });
	});
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

export class CoreWorker extends EventTarget {
	private readonly config: WorkerConfig;
	private readonly runtimeOptions: WorkerRuntimeOptions;
	private readonly cache: ElfCache;
	private readonly scheduler: TaskScheduler<TaskAssignPacket>;
	private readonly pendingResults: TaskResultPacket[] = [];
	private loopPromise: Promise<void> | null = null;
	private lifecycleController: AbortController | null = null;
	private ws: WebSocket | null = null;
	private token = '';
	private completedResults = 0;
	private idleAvailabilitySent = false;

	public constructor(config: WorkerConfig, runtimeOptions: WorkerRuntimeOptions = {}) {
		super();
		this.config = config;
		this.runtimeOptions = runtimeOptions;
		this.cache = new ElfCache({
			cacheDir: config.elfCacheDir,
			cacheLimitBytes: config.elfCacheLimitBytes,
		});
		this.scheduler = new TaskScheduler<TaskAssignPacket>(
			config.concurrency,
			async (task: ScheduledTask<TaskAssignPacket>) => {
				const signal = this.lifecycleController?.signal;
				if (signal == null) {
					return;
				}
				await this.executeTask(task.payload, signal);
			},
		);
		this.scheduler.onTaskError = (_task, error: unknown) => {
			const message = error instanceof Error ? error.message : String(error);
			this.emitWorkerEvent('runtime-error', `scheduler handler failure: ${message}`);
			workerLogger.error({ error: message }, 'scheduler handler failure');
		};
		this.scheduler.onIdle = () => {
			this.maybeSendIdleAvailability();
		};
	}

	private emitWorkerEvent<T extends keyof CoreWorkerEventDetailMap>(type: T, detail: CoreWorkerEventDetailMap[T]): void {
		this.dispatchEvent(new CustomEvent<CoreWorkerEventDetailMap[T]>(type, { detail }));
	}

	public async start(): Promise<void> {
		if (this.loopPromise != null) {
			return;
		}
		const controller = new AbortController();
		this.lifecycleController = controller;
		await this.cache.initialize();
		this.loopPromise = this.runConnectionLoop(controller.signal);
	}

	public async stop(): Promise<void> {
		this.lifecycleController?.abort(new Error('worker stopped'));
		if (this.ws != null) {
			this.ws.close();
		}
		if (this.loopPromise != null) {
			await this.loopPromise;
		}
		this.loopPromise = null;
		this.lifecycleController = null;
		this.ws = null;
	}

	private async runConnectionLoop(signal: AbortSignal): Promise<void> {
		let retryDelayMs = 1000;
		while (!signal.aborted) {
			try {
				this.token = await this.authenticate(signal);
				await this.connectAndServe(this.token, signal);
				retryDelayMs = 1000;
			} catch (error) {
				if (signal.aborted) {
					break;
				}
				const message = error instanceof Error ? error.message : String(error);
				this.emitWorkerEvent('runtime-error', message);
				workerLogger.error({ error: message }, 'worker reconnect');
				try {
					await sleepWithSignal(retryDelayMs, signal);
				} catch {
					if (signal.aborted) {
						break;
					}
					throw error;
				}
				retryDelayMs = Math.min(30000, retryDelayMs * 2);
			}
		}
	}

	private async authenticate(signal: AbortSignal): Promise<string> {
		signal.throwIfAborted();
		const url = buildEndpoint(this.config.backendUrl, 'api/worker/authenticate', false);
		const response = await fetch(url, {
			method: 'POST',
			signal,
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

	private async connectAndServe(token: string, signal: AbortSignal): Promise<void> {
		signal.throwIfAborted();
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

		const onAbort = () => {
			ws.close();
		};
		signal.addEventListener('abort', onAbort, { once: true });

		try {
			await new Promise<void>((resolve, reject) => {
				const cleanup = () => {
					ws.off('open', onOpen);
					ws.off('error', onError);
					signal.removeEventListener('abort', onConnectAbort);
				};

				const onOpen = () => {
					cleanup();
					resolve();
				};
				const onError = (error: Error) => {
					cleanup();
					reject(error);
				};
				const onConnectAbort = () => {
					cleanup();
					reject(createAbortError(signal));
				};

				ws.once('open', onOpen);
				ws.once('error', onError);
				signal.addEventListener('abort', onConnectAbort, { once: true });
			});

			this.sendPacket(new HelloPacket());
			this.replayUnfinishedAcknowledgements();
			this.flushPendingResults();
			this.maybeSendIdleAvailability();

			await new Promise<void>((resolve, reject) => {
				const cleanup = () => {
					ws.off('close', onClose);
					ws.off('error', onError);
					signal.removeEventListener('abort', onWaitAbort);
				};

				const onClose = () => {
					cleanup();
					resolve();
				};
				const onError = (error: Error) => {
					cleanup();
					reject(error);
				};
				const onWaitAbort = () => {
					cleanup();
					reject(createAbortError(signal));
				};

				ws.once('close', onClose);
				ws.once('error', onError);
				signal.addEventListener('abort', onWaitAbort, { once: true });
			});
		} finally {
			signal.removeEventListener('abort', onAbort);

			if (this.ws === ws) {
				this.ws = null;
			}
		}
	}

	private sendPacket(packet: Packet): void {
		if (this.ws == null || this.ws.readyState !== WebSocket.OPEN) {
			return;
		}
		const encoded = encodePacket(packet);
		this.ws.send(Buffer.from(encoded));
	}

	private computeCanAssignMore(): boolean {
		return this.scheduler.canAcceptMore();
	}

	private sendTaskAck(matchId: number): void {
		const packet = new TaskAckownledgedPacket();
		packet.matchId = matchId;
		packet.canAssignMore = this.computeCanAssignMore();
		this.sendPacket(packet);
	}

	private replayUnfinishedAcknowledgements(): void {
		for (const matchId of this.scheduler.getUnfinishedMatchIds()) {
			this.sendTaskAck(matchId);
		}
	}

	private maybeSendIdleAvailability(): void {
		if (this.idleAvailabilitySent) {
			return;
		}
		const snapshot = this.scheduler.snapshot();
		if (snapshot.runningCount > 0 || snapshot.queuedCount > 0) {
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
			this.emitWorkerEvent('decode-error', message);
			workerLogger.error({ error: message }, 'decode error');
			if (this.ws != null) {
				this.ws.close();
			}
			return;
		}

		if (!(packet instanceof TaskAssignPacket)) {
			return;
		}

		this.emitWorkerEvent('task-assigned', packet);
		this.idleAvailabilitySent = false;
		this.scheduler.push(packet.matchId, packet);
		this.sendTaskAck(packet.matchId);
	}

	private async executeTask(packet: TaskAssignPacket, signal: AbortSignal): Promise<void> {
		signal.throwIfAborted();
		let resultPacket: TaskResultPacket;
		let stderrCombined = '';
		try {
			const p1Descriptor = packet.strategies[0];
			const p2Descriptor = packet.strategies[1];
			const [p1BasePath, p1UnitPath, p2BasePath, p2UnitPath] = await Promise.all([
				this.cache.getOrDownload(p1Descriptor, 'base', signal),
				this.cache.getOrDownload(p1Descriptor, 'unit', signal),
				this.cache.getOrDownload(p2Descriptor, 'base', signal),
				this.cache.getOrDownload(p2Descriptor, 'unit', signal),
			]);
			const strategyPaths = {
				p1BasePath,
				p1UnitPath,
				p2BasePath,
				p2UnitPath,
			};

			const runResult = await runHeadless(this.config, {
				mapData: packet.mapData,
				strategies: strategyPaths,
			}, signal);
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

			try {
				await this.uploadReplay(packet.replayUploadUrl, runResult.stdout, signal);
			} catch (error) {
				const message = error instanceof Error ? error.message : String(error);
				stderrCombined += `\nfailed to upload replay: ${message}`;
				resultPacket = buildTaskCoreCrashResult(packet.matchId, packet.strategies, stderrCombined, this.config.errorLogMaxBytes);
				await this.dispatchTaskResult(resultPacket);
				return;
			}

			try {
				resultPacket = buildTaskSuccessResult(packet.matchId, packet.strategies, stderrCombined, this.config.errorLogMaxBytes);
			} catch (error) {
				const message = error instanceof Error ? error.message : String(error);
				stderrCombined += `\nfailed to parse worker jsonl result: ${message}`;
				resultPacket = buildTaskCoreCrashResult(packet.matchId, packet.strategies, stderrCombined, this.config.errorLogMaxBytes);
			}
		} catch (error) {
			if (signal.aborted) {
				return;
			}
			const message = error instanceof Error ? error.message : String(error);
			stderrCombined += `\nworker execution failure: ${message}`;
			resultPacket = buildTaskCoreCrashResult(packet.matchId, packet.strategies, stderrCombined, this.config.errorLogMaxBytes);
		}

		signal.throwIfAborted();
		await this.dispatchTaskResult(resultPacket);
	}

	private async uploadReplay(url: string, body: Buffer, signal: AbortSignal): Promise<void> {
		signal.throwIfAborted();
		const payload = new Uint8Array(body);
		const response = await fetch(url, {
			method: 'PUT',
			signal,
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
		this.emitWorkerEvent('task-result', packet);
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
