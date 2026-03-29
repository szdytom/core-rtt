import path from 'node:path';
import process from 'node:process';
import { config as loadDotenv } from 'dotenv';
import type { WorkerConfig } from './types.js';
import { pathExistsSync } from './fs-utils.js';

const DEFAULT_TIMEOUT_SECONDS = 1800;
const DEFAULT_MAX_TICKS = 5000;
const DEFAULT_ELF_CACHE_DIR = '/dev/shm/corertt-elf-cache';
const DEFAULT_TMP_MAP_DIR = '/dev/shm/corertt-tmp-map';
const DEFAULT_ELF_CACHE_LIMIT_BYTES = 200 * 1024 * 1024;
const DEFAULT_CONCURRENCY = 2;
const DEFAULT_ERROR_LOG_MAX_BYTES = 64 * 1024;

export const usage = [
	'Usage: corertt-core-worker',
	'',
	'Configuration is loaded from .env and process environment variables.',
	'',
	'Required keys:',
	'  CORERTT_WORKER_BACKEND_URL',
	'  CORERTT_WORKER_ID',
	'  CORERTT_WORKER_SECRET',
	'',
	'Optional keys:',
	'  CORERTT_WORKER_REPO_ROOT',
	'  CORERTT_WORKER_HEADLESS_PATH',
	'  CORERTT_WORKER_TIMEOUT_SECONDS',
	'  CORERTT_WORKER_MAX_TICKS',
	'  CORERTT_WORKER_ELF_CACHE_DIR',
	'  CORERTT_WORKER_ELF_CACHE_LIMIT',
	'  CORERTT_WORKER_TMP_MAP_DIR',
	'  CORERTT_WORKER_CONCURRENCY',
	'  CORERTT_WORKER_ERROR_LOG_MAX_BYTES',
	'  CORERTT_WORKER_DOWNLOAD_USE_BEARER',
	'  CORERTT_WORKER_ENV_PATH',
].join('\n');

function parsePositiveInt(raw: string, optionName: string): number {
	const value = Number.parseInt(raw, 10);
	if (!Number.isInteger(value) || value <= 0) {
		throw new Error(`${optionName} must be a positive integer`);
	}
	return value;
}

function parseByteSize(raw: string): number {
	const normalized = raw.trim().toUpperCase();
	const match = normalized.match(/^(\d+)([KMG])?B?$/);
	if (!match) {
		throw new Error('invalid byte size, expected formats like 200M or 1048576');
	}
	const value = Number.parseInt(match[1], 10);
	const unit = match[2] ?? '';
	if (unit === 'K') {
		return value * 1024;
	}
	if (unit === 'M') {
		return value * 1024 * 1024;
	}
	if (unit === 'G') {
		return value * 1024 * 1024 * 1024;
	}
	return value;
}

export function findRepoRoot(startDir: string): string {
	let currentDir = path.resolve(startDir);
	while (true) {
		const cmakePath = path.join(currentDir, 'CMakeLists.txt');
		const corelibPath = path.join(currentDir, 'corelib');
		const jsPath = path.join(currentDir, 'js');
		if (pathExistsSync(cmakePath) && pathExistsSync(corelibPath) && pathExistsSync(jsPath)) {
			return currentDir;
		}
		const parentDir = path.dirname(currentDir);
		if (parentDir === currentDir) {
			throw new Error('failed to auto-detect repository root, please set CORERTT_WORKER_REPO_ROOT');
		}
		currentDir = parentDir;
	}
}

function loadEnvironmentFile(repoRoot: string): void {
	const envPath = process.env.CORERTT_WORKER_ENV_PATH != null && process.env.CORERTT_WORKER_ENV_PATH.length > 0
		? path.resolve(process.env.CORERTT_WORKER_ENV_PATH)
		: path.join(repoRoot, 'js', 'core-worker', '.env');
	if (pathExistsSync(envPath)) {
		loadDotenv({ path: envPath, override: false });
	}
}

export function parseWorkerConfig(env: NodeJS.ProcessEnv = process.env): WorkerConfig {
	const repoRootFromEnv = env.CORERTT_WORKER_REPO_ROOT != null && env.CORERTT_WORKER_REPO_ROOT.length > 0
		? path.resolve(env.CORERTT_WORKER_REPO_ROOT)
		: findRepoRoot(process.cwd());

	loadEnvironmentFile(repoRootFromEnv);

	const backendUrl = String(process.env.CORERTT_WORKER_BACKEND_URL ?? '').trim();
	const workerId = String(process.env.CORERTT_WORKER_ID ?? '').trim();
	const workerSecret = String(process.env.CORERTT_WORKER_SECRET ?? '').trim();
	if (backendUrl.length === 0 || workerId.length === 0 || workerSecret.length === 0) {
		throw new Error('missing required env keys: CORERTT_WORKER_BACKEND_URL, CORERTT_WORKER_ID, CORERTT_WORKER_SECRET');
	}

	const repoRoot = process.env.CORERTT_WORKER_REPO_ROOT != null && process.env.CORERTT_WORKER_REPO_ROOT.length > 0
		? path.resolve(process.env.CORERTT_WORKER_REPO_ROOT)
		: repoRootFromEnv;

	const headlessPathRaw = String(process.env.CORERTT_WORKER_HEADLESS_PATH ?? '').trim();
	const headlessPath = headlessPathRaw.length > 0
		? path.resolve(headlessPathRaw)
		: path.join(repoRoot, 'build', 'corertt_headless');

	return {
		repoRoot,
		backendUrl,
		workerId,
		workerSecret,
		headlessPath,
		timeoutSeconds: parsePositiveInt(String(process.env.CORERTT_WORKER_TIMEOUT_SECONDS ?? DEFAULT_TIMEOUT_SECONDS), 'CORERTT_WORKER_TIMEOUT_SECONDS'),
		maxTicks: parsePositiveInt(String(process.env.CORERTT_WORKER_MAX_TICKS ?? DEFAULT_MAX_TICKS), 'CORERTT_WORKER_MAX_TICKS'),
		elfCacheDir: path.resolve(String(process.env.CORERTT_WORKER_ELF_CACHE_DIR ?? DEFAULT_ELF_CACHE_DIR)),
		elfCacheLimitBytes: parseByteSize(String(process.env.CORERTT_WORKER_ELF_CACHE_LIMIT ?? DEFAULT_ELF_CACHE_LIMIT_BYTES)),
		tmpMapDir: path.resolve(String(process.env.CORERTT_WORKER_TMP_MAP_DIR ?? DEFAULT_TMP_MAP_DIR)),
		concurrency: parsePositiveInt(String(process.env.CORERTT_WORKER_CONCURRENCY ?? DEFAULT_CONCURRENCY), 'CORERTT_WORKER_CONCURRENCY'),
		errorLogMaxBytes: parsePositiveInt(String(process.env.CORERTT_WORKER_ERROR_LOG_MAX_BYTES ?? DEFAULT_ERROR_LOG_MAX_BYTES), 'CORERTT_WORKER_ERROR_LOG_MAX_BYTES'),
		downloadUseBearer: String(process.env.CORERTT_WORKER_DOWNLOAD_USE_BEARER ?? '0') === '1',
	};
}
