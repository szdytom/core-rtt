import { createHash } from 'node:crypto';
import path from 'node:path';
import { readdir, rm, stat, unlink } from 'node:fs/promises';
import type { StrategyGroupDescriptor } from '@corertt/worker-codec';
import { ensureDir, pathExists, readJsonFile, writeFileAtomic } from './fs-utils.js';

interface CacheEntry {
	key: string;
	filePath: string;
	sizeBytes: number;
	lastAccessMs: number;
}

interface CacheIndexData {
	entries: CacheEntry[];
}

type StrategyKind = 'base' | 'unit';

function toDateValue(value: unknown): number {
	if (value instanceof Date) {
		return value.getTime();
	}
	if (typeof value === 'string' || typeof value === 'number') {
		return new Date(value).getTime();
	}
	return Number.NaN;
}

function buildCacheKey(descriptor: StrategyGroupDescriptor, kind: StrategyKind): string {
	const modified = kind === 'base'
		? toDateValue(descriptor.baseLastModified)
		: toDateValue(descriptor.unitLastModified);
	return `${descriptor.strategyGroupId}:${kind}:${modified}`;
}

function buildDownloadUrl(descriptor: StrategyGroupDescriptor, kind: StrategyKind): string {
	return kind === 'base' ? descriptor.baseStrategyUrl : descriptor.unitStrategyUrl;
}

function buildFileName(cacheKey: string, url: string): string {
	const hash = createHash('sha256').update(`${cacheKey}:${url}`).digest('hex');
	return `${hash}.elf`;
}

export class ElfCache {
	private readonly cacheDir: string;
	private readonly cacheLimitBytes: number;
	private readonly indexPath: string;
	private readonly entriesByKey = new Map<string, CacheEntry>();
	private readonly downloadUseBearer: boolean;
	private currentToken = '';
	private initialized = false;
	private inFlight = new Map<string, Promise<string>>();

	public constructor(options: {
		cacheDir: string;
		cacheLimitBytes: number;
		downloadUseBearer: boolean;
	}) {
		this.cacheDir = options.cacheDir;
		this.cacheLimitBytes = options.cacheLimitBytes;
		this.indexPath = path.join(this.cacheDir, 'index.json');
		this.downloadUseBearer = options.downloadUseBearer;
	}

	public setBearerToken(token: string): void {
		this.currentToken = token;
	}

	public async initialize(): Promise<void> {
		if (this.initialized) {
			return;
		}
		await ensureDir(this.cacheDir);
		const data = await readJsonFile<CacheIndexData>(this.indexPath);
		if (data?.entries != null) {
			for (const entry of data.entries) {
				if (await pathExists(entry.filePath)) {
					this.entriesByKey.set(entry.key, entry);
				}
			}
		}
		await this.reconcileEntries();
		await this.persistIndex();
		this.initialized = true;
	}

	public async getOrDownload(descriptor: StrategyGroupDescriptor, kind: StrategyKind): Promise<string> {
		await this.initialize();
		const cacheKey = buildCacheKey(descriptor, kind);
		const inFlight = this.inFlight.get(cacheKey);
		if (inFlight != null) {
			return inFlight;
		}

		const pending = this.getOrDownloadInternal(descriptor, kind, cacheKey);
		this.inFlight.set(cacheKey, pending);
		try {
			return await pending;
		} finally {
			this.inFlight.delete(cacheKey);
		}
	}

	private async getOrDownloadInternal(descriptor: StrategyGroupDescriptor, kind: StrategyKind, cacheKey: string): Promise<string> {
		const existing = this.entriesByKey.get(cacheKey);
		if (existing != null && await pathExists(existing.filePath)) {
			existing.lastAccessMs = Date.now();
			this.entriesByKey.set(cacheKey, existing);
			await this.persistIndex();
			return existing.filePath;
		}

		const url = buildDownloadUrl(descriptor, kind);
		if (url.length === 0) {
			throw new Error(`missing ${kind} strategy download url`);
		}

		const headers = new Headers();
		if (this.downloadUseBearer && this.currentToken.length > 0) {
			headers.set('Authorization', `Bearer ${this.currentToken}`);
		}

		const response = await fetch(url, { method: 'GET', headers });
		if (!response.ok) {
			throw new Error(`failed to download strategy ${kind}: ${response.status} ${response.statusText}`);
		}
		const buffer = Buffer.from(await response.arrayBuffer());
		const fileName = buildFileName(cacheKey, url);
		const filePath = path.join(this.cacheDir, fileName);
		await writeFileAtomic(filePath, buffer);

		const newEntry: CacheEntry = {
			key: cacheKey,
			filePath,
			sizeBytes: buffer.byteLength,
			lastAccessMs: Date.now(),
		};
		this.entriesByKey.set(cacheKey, newEntry);
		await this.evictIfNeeded();
		await this.persistIndex();

		const finalEntry = this.entriesByKey.get(cacheKey);
		if (finalEntry == null) {
			throw new Error('strategy was evicted immediately due to cache pressure');
		}
		return finalEntry.filePath;
	}

	private async reconcileEntries(): Promise<void> {
		for (const [key, entry] of this.entriesByKey.entries()) {
			if (!(await pathExists(entry.filePath))) {
				this.entriesByKey.delete(key);
				continue;
			}
			try {
				const fileStat = await stat(entry.filePath);
				entry.sizeBytes = fileStat.size;
				this.entriesByKey.set(key, entry);
			} catch {
				this.entriesByKey.delete(key);
			}
		}
	}

	private getCurrentSizeBytes(): number {
		let sum = 0;
		for (const entry of this.entriesByKey.values()) {
			sum += entry.sizeBytes;
		}
		return sum;
	}

	private async evictIfNeeded(): Promise<void> {
		let total = this.getCurrentSizeBytes();
		if (total <= this.cacheLimitBytes) {
			return;
		}

		const entries = Array.from(this.entriesByKey.values())
			.sort((left, right) => left.lastAccessMs - right.lastAccessMs);
		for (const entry of entries) {
			if (total <= this.cacheLimitBytes) {
				break;
			}
			this.entriesByKey.delete(entry.key);
			try {
				await unlink(entry.filePath);
			} catch {
				// Ignore file deletion errors during eviction.
			}
			total -= entry.sizeBytes;
		}
	}

	private async persistIndex(): Promise<void> {
		const payload: CacheIndexData = {
			entries: Array.from(this.entriesByKey.values()),
		};
		await writeFileAtomic(this.indexPath, JSON.stringify(payload, null, 2));
	}

	public async clear(): Promise<void> {
		await rm(this.cacheDir, { recursive: true, force: true });
		await ensureDir(this.cacheDir);
		this.entriesByKey.clear();
		await this.persistIndex();
	}

	public async debugListCacheFiles(): Promise<string[]> {
		await this.initialize();
		const entries = await readdir(this.cacheDir, { withFileTypes: true });
		return entries.filter((entry) => entry.isFile()).map((entry) => entry.name).sort();
	}
}
