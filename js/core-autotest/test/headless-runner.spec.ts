import path from 'node:path';
import { describe, expect, test } from 'vitest';
import { resolveCaseMapPath } from '../src/headless-runner.js';
import type { CaseSpec } from '../src/types.js';

function createSpec(overrides: Partial<CaseSpec>): CaseSpec {
	return {
		version: 1,
		name: 'map resolver test',
		timeLimitMs: 100,
		maxTicks: 10,
		program: [
			{ base: '@dummy', unit: '@dummy' },
			{ base: '@dummy', unit: '@dummy' },
		],
		...overrides,
	};
}

describe('resolveCaseMapPath', () => {
	test('resolves map filename under corelib/autotest/map', async () => {
		const repoRoot = path.resolve(__dirname, '../../..');
		const spec = createSpec({ map: 'tiny.txt' });
		const resolved = await resolveCaseMapPath(spec, repoRoot);
		expect(resolved).toBe(path.join(repoRoot, 'corelib', 'autotest', 'map', 'tiny.txt'));
	});

	test('rejects path traversal and separators', async () => {
		const repoRoot = path.resolve(__dirname, '../../..');
		const slashSpec = createSpec({ map: 'foo/bar.txt' });
		await expect(resolveCaseMapPath(slashSpec, repoRoot)).rejects.toThrow('map must be a filename under corelib/autotest/map');

		const parentSpec = createSpec({ map: '..' });
		await expect(resolveCaseMapPath(parentSpec, repoRoot)).rejects.toThrow('map filename is invalid: ..');
	});

	test('throws when map file does not exist', async () => {
		const repoRoot = path.resolve(__dirname, '../../..');
		const spec = createSpec({ map: 'not-found-map.txt' });
		await expect(resolveCaseMapPath(spec, repoRoot)).rejects.toThrow('map file not found');
	});
});
