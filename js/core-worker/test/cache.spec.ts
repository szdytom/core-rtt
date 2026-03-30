import { mkdtemp, readFile, stat, rm } from 'node:fs/promises';
import path from 'node:path';
import { tmpdir } from 'node:os';
import { afterEach, beforeEach, describe, expect, test, vi } from 'vitest';
import { StrategyGroupDescriptor } from '@corertt/worker-codec';
import { ElfCache } from '../src/cache.js';

function createDescriptor(overrides?: Partial<StrategyGroupDescriptor>): StrategyGroupDescriptor {
	const descriptor = new StrategyGroupDescriptor();
	descriptor.strategyGroupId = overrides?.strategyGroupId ?? 1;
	descriptor.baseLastModified = overrides?.baseLastModified ?? new Date('2026-01-01T00:00:00.000Z');
	descriptor.unitLastModified = overrides?.unitLastModified ?? new Date('2026-01-01T00:00:00.000Z');
	descriptor.baseStrategyUrl = overrides?.baseStrategyUrl ?? 'https://example.invalid/strategy/base';
	descriptor.unitStrategyUrl = overrides?.unitStrategyUrl ?? 'https://example.invalid/strategy/unit';
	return descriptor;
}

describe('ElfCache', () => {
	let cacheDir = '';
	let fetchMock: ReturnType<typeof vi.fn>;

	beforeEach(async () => {
		cacheDir = await mkdtemp(path.join(tmpdir(), 'corertt-cache-test-'));
		fetchMock = vi.fn(async (url: string) => {
			const payload = Buffer.from(`elf:${url}`, 'utf8');
			return new Response(payload, {
				status: 200,
				headers: {
					'Content-Type': 'application/octet-stream',
				},
			});
		});
		vi.stubGlobal('fetch', fetchMock);
	});

	afterEach(async () => {
		vi.unstubAllGlobals();
		if (cacheDir.length > 0) {
			await rm(cacheDir, { recursive: true, force: true });
		}
	});

	test('downloads once and reuses cache for repeated requests', async () => {
		const cache = new ElfCache({
			cacheDir,
			cacheLimitBytes: 64 * 1024 * 1024,
		});
		const descriptor = createDescriptor();

		const firstPath = await cache.getOrDownload(descriptor, 'base');
		const secondPath = await cache.getOrDownload(descriptor, 'base');

		expect(firstPath).toBe(secondPath);
		expect(fetchMock).toHaveBeenCalledTimes(1);
		expect((await stat(firstPath)).isFile()).toBe(true);
		const content = await readFile(firstPath);
		expect(content.toString('utf8')).toContain('/strategy/base');
	});

	test('deduplicates concurrent getOrDownload calls for same key', async () => {
		const cache = new ElfCache({
			cacheDir,
			cacheLimitBytes: 64 * 1024 * 1024,
		});
		const descriptor = createDescriptor();

		const [pathA, pathB] = await Promise.all([
			cache.getOrDownload(descriptor, 'unit'),
			cache.getOrDownload(descriptor, 'unit'),
		]);

		expect(pathA).toBe(pathB);
		expect(fetchMock).toHaveBeenCalledTimes(1);
	});

	test('persists index in sqlite and survives cache re-instantiation', async () => {
		const descriptor = createDescriptor({ strategyGroupId: 9 });
		const cacheA = new ElfCache({
			cacheDir,
			cacheLimitBytes: 64 * 1024 * 1024,
		});
		const firstPath = await cacheA.getOrDownload(descriptor, 'base');
		expect(fetchMock).toHaveBeenCalledTimes(1);

		const cacheB = new ElfCache({
			cacheDir,
			cacheLimitBytes: 64 * 1024 * 1024,
		});
		const secondPath = await cacheB.getOrDownload(descriptor, 'base');

		expect(secondPath).toBe(firstPath);
		expect(fetchMock).toHaveBeenCalledTimes(1);
	});
});
