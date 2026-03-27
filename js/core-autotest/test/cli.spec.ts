import path from 'node:path';
import { describe, expect, test } from 'vitest';
import { parseCliArgs } from '../src/cli.js';

describe('parseCliArgs', () => {
	test('uses both autotest directories by default', () => {
		const repo_root = '/repo-root';
		const parsed = parseCliArgs(['--repo-root', repo_root]);
		expect(parsed.autotest_dirs).toEqual([
			path.join(repo_root, 'corelib', 'autotest'),
			path.join(repo_root, 'rusty-corelib', 'autotest'),
		]);
	});

	test('accepts repeated --autotest-dir', () => {
		const parsed = parseCliArgs([
			'--repo-root', '/repo-root',
			'--autotest-dir', '/cases/a',
			'--autotest-dir', '/cases/b',
		]);
		expect(parsed.autotest_dirs).toEqual(['/cases/a', '/cases/b']);
	});
});
