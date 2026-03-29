import { spawn } from 'node:child_process';
import { mkdtemp, writeFile, rm } from 'node:fs/promises';
import path from 'node:path';
import { tmpdir } from 'node:os';
import type { HeadlessRunResult, StrategyBinaryRefs, WorkerConfig } from './types.js';
import { ensureDir } from './fs-utils.js';

export interface HeadlessTaskInput {
	mapData: ArrayBuffer;
	strategies: StrategyBinaryRefs;
}

async function maybePrepareMapFile(config: WorkerConfig, mapData: ArrayBuffer): Promise<{ mapFilePath: string | null; cleanup: () => Promise<void> }> {
	if (mapData.byteLength === 0) {
		return {
			mapFilePath: null,
			cleanup: async () => { },
		};
	}

	await ensureDir(config.tmpMapDir);
	const baseDir = path.resolve(config.tmpMapDir);
	const tempDir = await mkdtemp(path.join(baseDir, 'map-'));
	const mapFilePath = path.join(tempDir, 'mapfile.dat');
	await writeFile(mapFilePath, Buffer.from(mapData));
	return {
		mapFilePath,
		cleanup: async () => {
			await rm(tempDir, { recursive: true, force: true });
		},
	};
}

export async function runHeadless(config: WorkerConfig, task: HeadlessTaskInput): Promise<HeadlessRunResult> {
	const mapTemp = await maybePrepareMapFile(config, task.mapData);
	const args = [
		'--p1-base', task.strategies.p1BasePath,
		'--p1-unit', task.strategies.p1UnitPath,
		'--p2-base', task.strategies.p2BasePath,
		'--p2-unit', task.strategies.p2UnitPath,
		'--max-ticks', String(config.maxTicks),
	];
	if (mapTemp.mapFilePath != null) {
		args.push('--map', mapTemp.mapFilePath);
	}

	const child = spawn(config.headlessPath, args, {
		cwd: config.repoRoot,
		stdio: ['ignore', 'pipe', 'pipe'],
	});

	let timedOut = false;
	const stdoutChunks: Buffer[] = [];
	let stderrText = '';

	child.stdout?.on('data', (chunk: Buffer) => {
		stdoutChunks.push(chunk);
	});
	child.stderr?.on('data', (chunk: Buffer) => {
		stderrText += chunk.toString('utf8');
	});

	const timer = setTimeout(() => {
		timedOut = true;
		child.kill('SIGKILL');
	}, config.timeoutSeconds * 1000);

	const closeResult = await new Promise<{ code: number | null; signal: NodeJS.Signals | null }>((resolve, reject) => {
		child.once('error', reject);
		child.once('close', (code, signal) => resolve({ code, signal }));
	});

	clearTimeout(timer);
	await mapTemp.cleanup();

	return {
		stdout: Buffer.concat(stdoutChunks),
		stderr: stderrText,
		timedOut,
		exitCode: closeResult.code,
		exitSignal: closeResult.signal,
	};
}

export async function writeTempReplayFile(data: Buffer): Promise<string> {
	const tempDir = await mkdtemp(path.join(tmpdir(), 'corertt-replay-'));
	const replayPath = path.join(tempDir, 'replay.bin');
	await writeFile(replayPath, data);
	return replayPath;
}
