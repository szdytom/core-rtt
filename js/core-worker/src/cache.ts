import { createHash } from 'node:crypto';
import path from 'node:path';
import { readdir, rm, stat, unlink } from 'node:fs/promises';
import Database from 'better-sqlite3';
import type { StrategyGroupDescriptor } from '@corertt/worker-codec';
import { ensureDir, pathExists, writeFileAtomic } from './fs-utils.js';

interface CacheEntry {
	key: string;
	filePath: string;
	sizeBytes: number;
	lastAccessMs: number;
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
	private readonly indexDbPath: string;
	private currentToken = '';
	private initialized = false;
	private inFlight = new Map<string, Promise<string>>();
	private db: Database.Database | null = null;

	public constructor(options: {
		cacheDir: string;
		cacheLimitBytes: number;
	}) {
		this.cacheDir = options.cacheDir;
		this.cacheLimitBytes = options.cacheLimitBytes;
		this.indexDbPath = path.join(this.cacheDir, 'index.sqlite3');
	}

	public setBearerToken(token: string): void {
		this.currentToken = token;
	}

	public async initialize(): Promise<void> {
		if (this.initialized) {
			return;
		}
		await ensureDir(this.cacheDir);
		this.openDatabase();
		this.ensureSchema();
		await this.reconcileEntries();
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
		const existing = this.getEntryByKey(cacheKey);
		if (existing != null && await pathExists(existing.filePath)) {
			this.updateLastAccess(cacheKey, Date.now());
			return existing.filePath;
		}

		const url = buildDownloadUrl(descriptor, kind);
		if (url.length === 0) {
			throw new Error(`missing ${kind} strategy download url`);
		}

		const response = await fetch(url, { method: 'GET' });
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
		this.upsertEntry(newEntry);
		await this.evictIfNeeded();

		const finalEntry = this.getEntryByKey(cacheKey);
		if (finalEntry == null) {
			throw new Error('strategy was evicted immediately due to cache pressure');
		}
		return finalEntry.filePath;
	}

	private async reconcileEntries(): Promise<void> {
		for (const entry of this.listEntries()) {
			if (!(await pathExists(entry.filePath))) {
				this.deleteEntryByKey(entry.key);
				continue;
			}
			try {
				const fileStat = await stat(entry.filePath);
				this.updateEntrySize(entry.key, fileStat.size);
			} catch {
				this.deleteEntryByKey(entry.key);
			}
		}
	}

	private getCurrentSizeBytes(): number {
		const db = this.getDb();
		const row = db.prepare('SELECT COALESCE(SUM(size_bytes), 0) AS total FROM cache_entries').get() as { total: number };
		return Number(row.total);
	}

	private async evictIfNeeded(): Promise<void> {
		let total = this.getCurrentSizeBytes();
		if (total <= this.cacheLimitBytes) {
			return;
		}

		const entries = this.listEntriesByLru();
		for (const entry of entries) {
			if (total <= this.cacheLimitBytes) {
				break;
			}
			this.deleteEntryByKey(entry.key);
			try {
				await unlink(entry.filePath);
			} catch {
				// Ignore file deletion errors during eviction.
			}
			total -= entry.sizeBytes;
		}
	}

	private openDatabase(): void {
		if (this.db != null) {
			return;
		}
		this.db = new Database(this.indexDbPath);
		this.db.pragma('journal_mode = WAL');
		this.db.pragma('synchronous = NORMAL');
	}

	private ensureSchema(): void {
		const db = this.getDb();
		db.exec(
			`CREATE TABLE IF NOT EXISTS cache_entries (
				cache_key TEXT PRIMARY KEY,
				file_path TEXT NOT NULL,
				size_bytes INTEGER NOT NULL,
				last_access_ms INTEGER NOT NULL
			);
			CREATE INDEX IF NOT EXISTS idx_cache_entries_last_access
			ON cache_entries(last_access_ms);`,
		);
	}

	private getDb(): Database.Database {
		if (this.db == null) {
			throw new Error('cache database is not initialized');
		}
		return this.db;
	}

	private getEntryByKey(cacheKey: string): CacheEntry | null {
		const db = this.getDb();
		const row = db.prepare(
			`SELECT cache_key, file_path, size_bytes, last_access_ms
			 FROM cache_entries
			 WHERE cache_key = ?`,
		).get(cacheKey) as {
			cache_key: string;
			file_path: string;
			size_bytes: number;
			last_access_ms: number;
		} | undefined;
		if (row == null) {
			return null;
		}
		return {
			key: row.cache_key,
			filePath: row.file_path,
			sizeBytes: Number(row.size_bytes),
			lastAccessMs: Number(row.last_access_ms),
		};
	}

	private listEntries(): CacheEntry[] {
		const db = this.getDb();
		const rows = db.prepare(
			`SELECT cache_key, file_path, size_bytes, last_access_ms
			 FROM cache_entries`,
		).all() as Array<{
			cache_key: string;
			file_path: string;
			size_bytes: number;
			last_access_ms: number;
		}>;
		return rows.map((row) => ({
			key: row.cache_key,
			filePath: row.file_path,
			sizeBytes: Number(row.size_bytes),
			lastAccessMs: Number(row.last_access_ms),
		}));
	}

	private listEntriesByLru(): CacheEntry[] {
		const db = this.getDb();
		const rows = db.prepare(
			`SELECT cache_key, file_path, size_bytes, last_access_ms
			 FROM cache_entries
			 ORDER BY last_access_ms ASC`,
		).all() as Array<{
			cache_key: string;
			file_path: string;
			size_bytes: number;
			last_access_ms: number;
		}>;
		return rows.map((row) => ({
			key: row.cache_key,
			filePath: row.file_path,
			sizeBytes: Number(row.size_bytes),
			lastAccessMs: Number(row.last_access_ms),
		}));
	}

	private upsertEntry(entry: CacheEntry): void {
		const db = this.getDb();
		db.prepare(
			`INSERT INTO cache_entries(cache_key, file_path, size_bytes, last_access_ms)
			 VALUES (?, ?, ?, ?)
			 ON CONFLICT(cache_key) DO UPDATE SET
				file_path = excluded.file_path,
				size_bytes = excluded.size_bytes,
				last_access_ms = excluded.last_access_ms`,
		).run(entry.key, entry.filePath, entry.sizeBytes, entry.lastAccessMs);
	}

	private updateLastAccess(cacheKey: string, lastAccessMs: number): void {
		const db = this.getDb();
		db.prepare(
			`UPDATE cache_entries
			 SET last_access_ms = ?
			 WHERE cache_key = ?`,
		).run(lastAccessMs, cacheKey);
	}

	private updateEntrySize(cacheKey: string, sizeBytes: number): void {
		const db = this.getDb();
		db.prepare(
			`UPDATE cache_entries
			 SET size_bytes = ?
			 WHERE cache_key = ?`,
		).run(sizeBytes, cacheKey);
	}

	private deleteEntryByKey(cacheKey: string): void {
		const db = this.getDb();
		db.prepare('DELETE FROM cache_entries WHERE cache_key = ?').run(cacheKey);
	}

	private closeDatabase(): void {
		if (this.db == null) {
			return;
		}
		this.db.close();
		this.db = null;
	}

	public async clear(): Promise<void> {
		this.closeDatabase();
		await rm(this.cacheDir, { recursive: true, force: true });
		await ensureDir(this.cacheDir);
		this.inFlight.clear();
		this.initialized = false;
		await this.initialize();
	}

	public async debugListCacheFiles(): Promise<string[]> {
		await this.initialize();
		const entries = await readdir(this.cacheDir, { withFileTypes: true });
		return entries.filter((entry) => entry.isFile()).map((entry) => entry.name).sort();
	}
}
