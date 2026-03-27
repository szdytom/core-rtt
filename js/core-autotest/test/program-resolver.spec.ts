import { mkdtemp, mkdir, rm, writeFile } from 'fs/promises';
import os from 'os';
import path from 'path';
import { afterEach, describe, expect, test } from 'vitest';
import { parseCargoExampleExecutables, resolveProgramRef } from '../src/program-resolver.js';
import type { CaseSpec } from '../src/types.js';

const temp_dirs: string[] = [];

function makeSpec(): CaseSpec {
	return {
		version: 1,
		name: 'resolver test',
		timeLimitMs: 100,
		maxTicks: 10,
		program: [
			{ base: '@dummy', unit: '@dummy' },
			{ base: '@dummy', unit: '@dummy' },
		],
	};
}

afterEach(async () => {
	for (const dir_path of temp_dirs.splice(0)) {
		await rm(dir_path, { recursive: true, force: true });
	}
});

describe('resolveProgramRef', () => {
	test('parses cargo JSON output and keeps executable entries', () => {
		const parsed = parseCargoExampleExecutables([
			'{"reason":"build-script-executed"}',
			'{"reason":"compiler-artifact","target":{"name":"log_simple","crate_types":["bin"]},"executable":"/tmp/log_simple"}',
			'{"reason":"compiler-artifact","target":{"name":"log_simple","crate_types":["bin"]},"executable":"/tmp/log_simple_dup"}',
			'{"reason":"compiler-artifact","target":{"name":"rusty_corelib","crate_types":["rlib"]},"executable":null}',
		].join('\n'));

		expect(parsed.get('log_simple')).toEqual(['/tmp/log_simple', '/tmp/log_simple_dup']);
		expect(parsed.has('rusty_corelib')).toBe(false);
	});

	test('resolves Rust bare name from cargo JSON output', async () => {
		const repo_root = await mkdtemp(path.join(os.tmpdir(), 'corertt-resolver-'));
		temp_dirs.push(repo_root);
		const rust_root = path.join(repo_root, 'rusty-corelib');
		const fake_bin_dir = path.join(repo_root, 'fake-bin');
		await mkdir(rust_root, { recursive: true });
		await mkdir(fake_bin_dir, { recursive: true });

		const rust_bin = path.join(repo_root, 'target-from-cargo', 'log_simple');
		await mkdir(path.dirname(rust_bin), { recursive: true });
		await writeFile(rust_bin, '');

		const cargo_script = path.join(fake_bin_dir, 'cargo');
		const cargo_script_content = [
			'#!/bin/sh',
			'echo "{\\"reason\\":\\"compiler-artifact\\",\\"target\\":{\\"name\\":\\"log_simple\\",\\"crate_types\\":[\\"bin\\"]},\\"executable\\":\\"' + rust_bin.replaceAll('"', '\\"') + '\\"}"',
		].join('\n');
		await writeFile(cargo_script, cargo_script_content, { mode: 0o755 });

		const old_path = process.env.PATH ?? '';
		process.env.PATH = `${fake_bin_dir}:${old_path}`;
		try {
			const resolved = await resolveProgramRef('rusty::log_simple', makeSpec(), repo_root);
			expect(resolved).toBe(rust_bin);
		} finally {
			process.env.PATH = old_path;
		}
	});

	test('throws when C namespace has multiple matches', async () => {
		const repo_root = await mkdtemp(path.join(os.tmpdir(), 'corertt-resolver-'));
		temp_dirs.push(repo_root);
		const c_autotest_dir = path.join(repo_root, 'corelib', 'autotest');
		const c_test_dir = path.join(repo_root, 'corelib', 'test');
		await mkdir(c_autotest_dir, { recursive: true });
		await mkdir(c_test_dir, { recursive: true });
		const c_elf = path.join(c_autotest_dir, 'same_name.elf');
		const c_test_elf = path.join(c_test_dir, 'same_name.elf');
		await writeFile(c_elf, '');
		await writeFile(c_test_elf, '');

		await expect(resolveProgramRef('c::same_name', makeSpec(), repo_root)).rejects.toThrow('Ambiguous program reference');
	});

	test('requires namespace prefix for symbolic refs', async () => {
		const repo_root = await mkdtemp(path.join(os.tmpdir(), 'corertt-resolver-'));
		temp_dirs.push(repo_root);

		await expect(resolveProgramRef('log_simple', makeSpec(), repo_root)).rejects.toThrow('must use c:: or rusty::');
	});
});
