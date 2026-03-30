import { mkdtemp, rm, access } from 'node:fs/promises';
import { constants as fsConstants } from 'node:fs';
import path from 'node:path';
import { tmpdir } from 'node:os';
import { fileURLToPath } from 'node:url';
import { describe, expect, test } from 'vitest';
import { MatchResult, TaskStatus } from '@corertt/worker-codec';
import { CoreWorker } from '../src/worker.js';
import { MockWorkerBackend } from '../src/mock-server.js';
import type { WorkerConfig } from '../src/types.js';

function findRepoRoot(): string {
	const currentFile = fileURLToPath(import.meta.url);
	const coreWorkerDir = path.dirname(path.dirname(currentFile));
	return path.resolve(coreWorkerDir, '..', '..');
}

async function pathExists(pathValue: string): Promise<boolean> {
	try {
		await access(pathValue, fsConstants.F_OK);
		return true;
	} catch {
		return false;
	}
}

describe('core-worker integration', () => {
	test('executes task, uploads zstd replay and returns TaskResult', async () => {
		const repoRoot = findRepoRoot();
		const headlessPath = path.join(repoRoot, 'build', 'corertt_headless');
		const dummyElf = path.join(repoRoot, 'corelib', 'test', 'dummy.elf');

		if (!(await pathExists(headlessPath)) || !(await pathExists(dummyElf))) {
			return;
		}

		const cacheDir = await mkdtemp(path.join(tmpdir(), 'corertt-cache-'));
		const tmpMapDir = await mkdtemp(path.join(tmpdir(), 'corertt-map-'));
		const server = new MockWorkerBackend({
			requiredWorkerId: 'worker-1',
			requiredSecret: 'secret-1',
			strategyFiles: {
				p1Base: dummyElf,
				p1Unit: dummyElf,
				p2Base: dummyElf,
				p2Unit: dummyElf,
			},
			taskMatchId: 101,
		});

		await server.start();
		const config: WorkerConfig = {
			repoRoot,
			backendUrl: server.baseUrl,
			workerId: 'worker-1',
			workerSecret: 'secret-1',
			headlessPath,
			timeoutSeconds: 30,
			maxTicks: 100,
			elfCacheDir: cacheDir,
			elfCacheLimitBytes: 200 * 1024 * 1024,
			tmpMapDir,
			concurrency: 2,
			errorLogMaxBytes: 64 * 1024,
		};

		const decodeErrors: string[] = [];
		const runtimeErrors: string[] = [];
		const assignedMatchIds: number[] = [];
		const worker = new CoreWorker(config);
		worker.addEventListener('decode-error', (event) => {
			decodeErrors.push((event as CustomEvent<string>).detail);
		});
		worker.addEventListener('runtime-error', (event) => {
			runtimeErrors.push((event as CustomEvent<string>).detail);
		});
		worker.addEventListener('task-assigned', (event) => {
			assignedMatchIds.push((event as CustomEvent<{ matchId: number }>).detail.matchId);
		});
		await worker.start();
		let result;
		try {
			result = await server.waitForResult(60000);
		} catch (error) {
			const message = error instanceof Error ? error.message : String(error);
			throw new Error(`${message}; assigned=${assignedMatchIds.join(',')}; decodeErrors=${decodeErrors.join(' | ')}; runtimeErrors=${runtimeErrors.join(' | ')}`);
		}
		await worker.stop();
		await server.stop();

		expect(result.matchId).toBe(101);
		expect([TaskStatus.Success, TaskStatus.CoreCrash]).toContain(result.status);
		expect([MatchResult.P1Win, MatchResult.P2Win, MatchResult.Tied, MatchResult.NoResult]).toContain(result.result);

		const replayUpload = server.getUploadedReplay(101);
		expect(replayUpload).not.toBeNull();
		expect((replayUpload ?? Buffer.alloc(0)).byteLength).toBeGreaterThan(0);

		const acks = server.getReceivedAcks();
		expect(acks.some((item) => item.matchId === 101)).toBe(true);

		await rm(cacheDir, { recursive: true, force: true });
		await rm(tmpMapDir, { recursive: true, force: true });
	}, 90000);
});
