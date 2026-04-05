import { createServer, type IncomingMessage, type ServerResponse } from 'node:http';
import { readFile } from 'node:fs/promises';
import { URL } from 'node:url';
import { WebSocketServer, type WebSocket } from 'ws';
import {
	CODEC_VERSION,
	PROTOCOL_MAGIC,
	decodePacket,
	encodePacket,
	isSnowflakeId,
	type SnowflakeId,
	TaskAssignPacket,
	TaskAcknowledgedPacket,
	TaskResultPacket,
	StrategyGroupDescriptor,
} from '@corertt/worker-codec';

interface MockServerOptions {
	port?: number;
	requiredWorkerId: string;
	requiredSecret: string;
	strategyFiles: {
		p1Base: string;
		p1Unit: string;
		p2Base: string;
		p2Unit: string;
	};
	taskMatchId: SnowflakeId;
	mapData?: ArrayBuffer;
}

function toArrayBuffer(data: import('ws').RawData): ArrayBuffer {
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

function encodeHelloPacket(): Buffer {
	const buffer = new ArrayBuffer(8);
	const view = new DataView(buffer);
	view.setUint16(0, PROTOCOL_MAGIC, true);
	view.setUint8(2, CODEC_VERSION);
	view.setUint8(3, 1);
	view.setUint32(4, 0, true);
	return Buffer.from(buffer);
}

export class MockWorkerBackend {
	private readonly options: MockServerOptions;
	private readonly server = createServer(this.handleHttpRequest.bind(this));
	private readonly wsServer = new WebSocketServer({ noServer: true });
	private readonly uploads = new Map<SnowflakeId, Buffer>();
	private readonly acknowledgements: TaskAcknowledgedPacket[] = [];
	private readonly results: TaskResultPacket[] = [];
	private readonly token = 'mock-token';
	private started = false;
	private expectedTaskSent = false;
	private taskSentCount = 0;
	private lastTaskSendError = '';
	private readonly waiters: Array<(value: TaskResultPacket) => void> = [];

	public constructor(options: MockServerOptions) {
		this.options = options;
		this.server.on('upgrade', (request, socket, head) => {
			if (request.url !== '/api/worker/communication') {
				socket.destroy();
				return;
			}
			const auth = request.headers.authorization ?? '';
			if (auth !== `Bearer ${this.token}`) {
				socket.destroy();
				return;
			}
			this.wsServer.handleUpgrade(request, socket, head, (ws) => {
				this.wsServer.emit('connection', ws, request);
			});
		});
		this.wsServer.on('connection', (ws) => {
			this.handleWorkerConnection(ws);
		});
	}

	public async start(): Promise<number> {
		if (this.started) {
			return this.port;
		}
		await new Promise<void>((resolve) => {
			this.server.listen(this.options.port ?? 0, '127.0.0.1', () => resolve());
		});
		this.started = true;
		return this.port;
	}

	public async stop(): Promise<void> {
		await new Promise<void>((resolve) => {
			this.wsServer.close(() => resolve());
		});
		await new Promise<void>((resolve, reject) => {
			this.server.close((error) => {
				if (error != null) {
					reject(error);
					return;
				}
				resolve();
			});
		});
	}

	public get port(): number {
		const address = this.server.address();
		if (address == null || typeof address === 'string') {
			throw new Error('mock server is not listening');
		}
		return address.port;
	}

	public get baseUrl(): string {
		return `http://127.0.0.1:${this.port}`;
	}

	public getReceivedAcks(): TaskAcknowledgedPacket[] {
		return [...this.acknowledgements];
	}

	public getUploadedReplay(matchId: SnowflakeId): Buffer | null {
		return this.uploads.get(matchId) ?? null;
	}

	public async waitForResult(timeoutMs: number): Promise<TaskResultPacket> {
		if (this.results.length > 0) {
			return this.results[this.results.length - 1];
		}
		return await new Promise<TaskResultPacket>((resolve, reject) => {
			const timer = setTimeout(() => {
				const ackIds = this.acknowledgements.map((item) => item.matchId).join(',');
				reject(new Error(`timed out waiting for task result (acks=${this.acknowledgements.length} [${ackIds}], results=${this.results.length}, uploads=${this.uploads.size}, taskSent=${this.taskSentCount}, taskSendError=${this.lastTaskSendError})`));
			}, timeoutMs);
			this.waiters.push((packet) => {
				clearTimeout(timer);
				resolve(packet);
			});
		});
	}

	private async handleHttpRequest(request: IncomingMessage, response: ServerResponse): Promise<void> {
		const requestUrl = new URL(request.url ?? '/', 'http://127.0.0.1');
		if (request.method === 'POST' && requestUrl.pathname === '/api/worker/authenticate') {
			await this.handleAuthenticate(request, response);
			return;
		}
		if (request.method === 'GET' && requestUrl.pathname.startsWith('/api/mock/strategy/')) {
			await this.handleStrategyDownload(requestUrl.pathname, response);
			return;
		}
		if (request.method === 'PUT' && requestUrl.pathname.startsWith('/api/mock/replay/')) {
			await this.handleReplayUpload(requestUrl.pathname, request, response);
			return;
		}

		response.statusCode = 404;
		response.end('not found');
	}

	private async handleAuthenticate(request: IncomingMessage, response: ServerResponse): Promise<void> {
		const chunks: Buffer[] = [];
		for await (const chunk of request) {
			chunks.push(Buffer.from(chunk));
		}
		const body = JSON.parse(Buffer.concat(chunks).toString('utf8')) as { worker_id?: string; secret?: string };
		if (body.worker_id !== this.options.requiredWorkerId || body.secret !== this.options.requiredSecret) {
			response.statusCode = 401;
			response.end('unauthorized');
			return;
		}
		response.setHeader('Content-Type', 'application/json');
		response.end(JSON.stringify({ token: this.token }));
	}

	private async handleStrategyDownload(pathname: string, response: ServerResponse): Promise<void> {
		const fileKey = pathname.split('/').pop();
		const mapping = {
			p1_base: this.options.strategyFiles.p1Base,
			p1_unit: this.options.strategyFiles.p1Unit,
			p2_base: this.options.strategyFiles.p2Base,
			p2_unit: this.options.strategyFiles.p2Unit,
		} as const;
		if (fileKey == null || !(fileKey in mapping)) {
			response.statusCode = 404;
			response.end('unknown strategy key');
			return;
		}
		const payload = await readFile(mapping[fileKey as keyof typeof mapping]);
		response.statusCode = 200;
		response.setHeader('Content-Type', 'application/octet-stream');
		response.end(payload);
	}

	private async handleReplayUpload(pathname: string, request: IncomingMessage, response: ServerResponse): Promise<void> {
		const matchId = pathname.split('/').pop() ?? '';
		if (!isSnowflakeId(matchId)) {
			response.statusCode = 400;
			response.end('invalid match id');
			return;
		}

		const chunks: Buffer[] = [];
		for await (const chunk of request) {
			chunks.push(Buffer.from(chunk));
		}
		this.uploads.set(matchId, Buffer.concat(chunks));
		response.statusCode = 200;
		response.end('ok');
	}

	private handleWorkerConnection(ws: WebSocket): void {
		ws.send(encodeHelloPacket());

		if (!this.expectedTaskSent) {
			this.expectedTaskSent = true;
			const packet = new TaskAssignPacket();
			packet.matchId = this.options.taskMatchId;
			packet.mapData = this.options.mapData ?? new ArrayBuffer(0);
			packet.strategies = [
				this.createStrategy('100000000001', `${this.baseUrl}/api/mock/strategy/p1_base`, `${this.baseUrl}/api/mock/strategy/p1_unit`),
				this.createStrategy('100000000002', `${this.baseUrl}/api/mock/strategy/p2_base`, `${this.baseUrl}/api/mock/strategy/p2_unit`),
			];
			packet.replayUploadUrl = `${this.baseUrl}/api/mock/replay/${packet.matchId}`;
			try {
				ws.send(Buffer.from(encodePacket(packet)));
				this.taskSentCount += 1;
			} catch (error) {
				this.lastTaskSendError = error instanceof Error ? error.message : String(error);
			}
		}

		ws.on('message', (data) => {
			const packet = decodePacket(toArrayBuffer(data));
			if (packet instanceof TaskAcknowledgedPacket) {
				this.acknowledgements.push(packet);
				return;
			}
			if (packet instanceof TaskResultPacket) {
				this.results.push(packet);
				const waiter = this.waiters.shift();
				if (waiter != null) {
					waiter(packet);
				}
			}
		});
	}

	private createStrategy(groupId: SnowflakeId, baseUrl: string, unitUrl: string): StrategyGroupDescriptor {
		const descriptor = new StrategyGroupDescriptor();
		descriptor.strategyGroupId = groupId;
		descriptor.baseLastModified = new Date('2026-01-01T00:00:00.000Z');
		descriptor.unitLastModified = new Date('2026-01-01T00:00:00.000Z');
		descriptor.baseStrategyUrl = baseUrl;
		descriptor.unitStrategyUrl = unitUrl;
		return descriptor;
	}
}
