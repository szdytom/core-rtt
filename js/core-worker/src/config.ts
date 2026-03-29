import { parseArgs } from 'node:util';
import path from 'node:path';
import process from 'node:process';
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
	'Usage: corertt-core-worker [options]',
	'',
	'Required:',
	'  --backend-url <url>       backend base url, e.g. http://127.0.0.1:3000',
	'  --worker-id <id>          worker id',
	'  --worker-secret <secret>  worker secret',
	'',
	'Optional:',
	'  --repo-root <path>            repository root (auto-detected when omitted)',
	'  --headless-path <path>        corertt_headless path (default: <repo>/build/corertt_headless)',
	'  --timeout-seconds <n>         process timeout in seconds (default: 1800)',
	'  --max-ticks <n>               max ticks passed to headless (default: 5000)',
	'  --elf-cache-dir <path>        ELF cache dir (default: /dev/shm/corertt-elf-cache)',
	'  --elf-cache-limit <bytes|200M> total ELF cache size limit (default: 200M)',
	'  --tmp-map-dir <path>          temp map directory (default: /dev/shm/corertt-tmp-map)',
	'  --concurrency <n>             parallel task count (default: 2)',
	'  --error-log-max-bytes <n>     stderr cap in TaskResult.errorLog (default: 65536)',
	'  --download-use-bearer         attach bearer token while downloading ELF',
	'  -h, --help                    show help',
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
			throw new Error('failed to auto-detect repository root, please pass --repo-root');
		}
		currentDir = parentDir;
	}
}

export function parseWorkerConfig(argv: string[], env: NodeJS.ProcessEnv = process.env): WorkerConfig {
	const parsed = parseArgs({
		args: argv,
		allowPositionals: false,
		strict: true,
		options: {
			'repo-root': { type: 'string', default: '' },
			'backend-url': { type: 'string', default: env.CORERTT_WORKER_BACKEND_URL ?? '' },
			'worker-id': { type: 'string', default: env.CORERTT_WORKER_ID ?? '' },
			'worker-secret': { type: 'string', default: env.CORERTT_WORKER_SECRET ?? '' },
			'headless-path': { type: 'string', default: env.CORERTT_WORKER_HEADLESS_PATH ?? '' },
			'timeout-seconds': { type: 'string', default: env.CORERTT_WORKER_TIMEOUT_SECONDS ?? String(DEFAULT_TIMEOUT_SECONDS) },
			'max-ticks': { type: 'string', default: env.CORERTT_WORKER_MAX_TICKS ?? String(DEFAULT_MAX_TICKS) },
			'elf-cache-dir': { type: 'string', default: env.CORERTT_WORKER_ELF_CACHE_DIR ?? DEFAULT_ELF_CACHE_DIR },
			'elf-cache-limit': { type: 'string', default: env.CORERTT_WORKER_ELF_CACHE_LIMIT ?? String(DEFAULT_ELF_CACHE_LIMIT_BYTES) },
			'tmp-map-dir': { type: 'string', default: env.CORERTT_WORKER_TMP_MAP_DIR ?? DEFAULT_TMP_MAP_DIR },
			'concurrency': { type: 'string', default: env.CORERTT_WORKER_CONCURRENCY ?? String(DEFAULT_CONCURRENCY) },
			'error-log-max-bytes': { type: 'string', default: env.CORERTT_WORKER_ERROR_LOG_MAX_BYTES ?? String(DEFAULT_ERROR_LOG_MAX_BYTES) },
			'download-use-bearer': { type: 'boolean', default: env.CORERTT_WORKER_DOWNLOAD_USE_BEARER === '1' },
			help: { type: 'boolean', short: 'h' },
		},
	});

	if (parsed.values.help) {
		process.stdout.write(`${usage}\n`);
		process.exit(0);
	}

	const backendUrl = String(parsed.values['backend-url'] ?? '').trim();
	const workerId = String(parsed.values['worker-id'] ?? '').trim();
	const workerSecret = String(parsed.values['worker-secret'] ?? '').trim();
	if (backendUrl.length === 0 || workerId.length === 0 || workerSecret.length === 0) {
		throw new Error('missing required options: --backend-url, --worker-id, --worker-secret');
	}

	const repoRoot = String(parsed.values['repo-root']).length > 0
		? path.resolve(String(parsed.values['repo-root']))
		: findRepoRoot(process.cwd());

	const headlessPathRaw = String(parsed.values['headless-path'] ?? '').trim();
	const headlessPath = headlessPathRaw.length > 0
		? path.resolve(headlessPathRaw)
		: path.join(repoRoot, 'build', 'corertt_headless');

	return {
		repoRoot,
		backendUrl,
		workerId,
		workerSecret,
		headlessPath,
		timeoutSeconds: parsePositiveInt(String(parsed.values['timeout-seconds']), '--timeout-seconds'),
		maxTicks: parsePositiveInt(String(parsed.values['max-ticks']), '--max-ticks'),
		elfCacheDir: path.resolve(String(parsed.values['elf-cache-dir'])),
		elfCacheLimitBytes: parseByteSize(String(parsed.values['elf-cache-limit'])),
		tmpMapDir: path.resolve(String(parsed.values['tmp-map-dir'])),
		concurrency: parsePositiveInt(String(parsed.values.concurrency), '--concurrency'),
		errorLogMaxBytes: parsePositiveInt(String(parsed.values['error-log-max-bytes']), '--error-log-max-bytes'),
		downloadUseBearer: Boolean(parsed.values['download-use-bearer']),
	};
}
