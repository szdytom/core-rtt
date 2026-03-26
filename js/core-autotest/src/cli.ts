import { parseArgs } from 'node:util';
import path from 'node:path';
import process from 'node:process';
import type { CliOptions } from './types.js';
import { pathExistsSync } from './fs-utils.js';

export const usage = [
	'Usage: corertt-core-autotest [options]',
	'',
	'Options:',
	'  --repo-root <path>      repository root (auto-detected when omitted)',
	'  --autotest-dir <path>   autotest directory (default: <repo>/corelib/autotest)',
	'  --headless <path>       corertt_headless binary path (default: <repo>/build/corertt_headless)',
	'  --case <name>           only run case by exact "name" field',
	'  --fail-fast             stop after first failed case',
	'  --verbose               print headless command for each run',
	'  -h, --help              show this help',
].join('\n');

export function parseCliArgs(argv: string[]): CliOptions {
	const parsed = parseArgs({
		args: argv,
		options: {
			'repo-root': { type: 'string', default: '' },
			'autotest-dir': { type: 'string', default: '' },
			headless: { type: 'string', default: '' },
			case: { type: 'string', default: '' },
			'fail-fast': { type: 'boolean', default: false },
			verbose: { type: 'boolean', default: false },
			help: { type: 'boolean', short: 'h' },
		},
		allowPositionals: false,
		strict: true,
	});

	if (parsed.values.help) {
		process.stdout.write(`${usage}\n`);
		process.exit(0);
	}

	const repo_root = typeof parsed.values['repo-root'] === 'string' && parsed.values['repo-root'].length > 0
		? path.resolve(parsed.values['repo-root'])
		: findRepoRoot(process.cwd());
	const autotest_dir = typeof parsed.values['autotest-dir'] === 'string' && parsed.values['autotest-dir'].length > 0
		? path.resolve(parsed.values['autotest-dir'])
		: path.join(repo_root, 'corelib', 'autotest');
	const headless_path = typeof parsed.values.headless === 'string' && parsed.values.headless.length > 0
		? path.resolve(parsed.values.headless)
		: path.join(repo_root, 'build', 'corertt_headless');

	return {
		repo_root,
		autotest_dir,
		headless_path,
		case_name: String(parsed.values.case ?? ''),
		fail_fast: Boolean(parsed.values['fail-fast']),
		verbose: Boolean(parsed.values.verbose),
	};
}

export function findRepoRoot(start_dir: string): string {
	let current_dir = path.resolve(start_dir);
	while (true) {
		const cmake_path = path.join(current_dir, 'CMakeLists.txt');
		const corelib_path = path.join(current_dir, 'corelib');
		const js_path = path.join(current_dir, 'js');
		if (pathExistsSync(cmake_path) && pathExistsSync(corelib_path) && pathExistsSync(js_path)) {
			return current_dir;
		}

		const parent_dir = path.dirname(current_dir);
		if (parent_dir === current_dir) {
			throw new Error('Failed to auto-detect repository root. Please pass --repo-root.');
		}
		current_dir = parent_dir;
	}
}
