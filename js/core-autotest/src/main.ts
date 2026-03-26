#!/usr/bin/env node

import { pathToFileURL } from 'node:url';
import process from 'node:process';
import { usage, parseCliArgs } from './cli.js';
import type { CaseResult, CliOptions, TestCase } from './types.js';
import { pathExists } from './fs-utils.js';
import { loadCases } from './case-spec.js';
import { runHeadlessOnce } from './headless-runner.js';
import { formatLog } from './assertions.js';

async function runCase(test_case: TestCase, options: CliOptions): Promise<CaseResult> {
	const runs = test_case.spec.runs ?? 1;
	let runs_passed = 0;
	const failures: string[] = [];

	for (let run_index = 0; run_index < runs; run_index++) {
		const result = await runHeadlessOnce(
			test_case.spec,
			options.repo_root,
			options.headless_path,
			options.verbose,
		);
		if (test_case.spec.expectFailure === true) {
			if (result.ok) {
				const detail = [
					`FAIL ${test_case.spec.name} [run ${run_index + 1}/${runs}]`,
					'  reason: expectFailure=true but run passed',
				].join('\n');
				process.stdout.write(`${detail}\n`);
				failures.push(detail);
				continue;
			}

			runs_passed += 1;
			process.stdout.write(`PASS ${test_case.spec.name} [run ${run_index + 1}/${runs}] (expected failure observed)\n`);
			continue;
		}

		if (result.ok) {
			runs_passed += 1;
			process.stdout.write(`PASS ${test_case.spec.name} [run ${run_index + 1}/${runs}]\n`);
			continue;
		}

		const lines = [
			`FAIL ${test_case.spec.name} [run ${run_index + 1}/${runs}]`,
			`  reason: ${result.failure?.message ?? 'unknown failure'}`,
		];
		if (result.failure?.offending_log != null) {
			lines.push(`  offending_log: ${formatLog(result.failure.offending_log)}`);
		}
		if (result.stderr_text.trim().length > 0) {
			lines.push(`  stderr: ${result.stderr_text.trim()}`);
		}
		const detail = lines.join('\n');
		process.stdout.write(`${detail}\n`);
		failures.push(detail);
	}

	return {
		ok: runs_passed === runs,
		name: test_case.spec.name,
		file_path: test_case.file_path,
		runs_total: runs,
		runs_passed,
		failures,
	};
}

async function run(argv: string[]): Promise<number> {
	const options = parseCliArgs(argv);

	if (!(await pathExists(options.headless_path))) {
		throw new Error(`corertt_headless not found: ${options.headless_path}`);
	}
	if (!(await pathExists(options.autotest_dir))) {
		throw new Error(`autotest directory not found: ${options.autotest_dir}`);
	}

	const cases = await loadCases(options.autotest_dir, options.case_name);
	if (cases.length === 0) {
		process.stdout.write('Summary: total=0 passed=0 failed=1\n');
		process.stdout.write('  - No test case found\n');
		return 1;
	}

	const case_results: CaseResult[] = [];
	for (const test_case of cases) {
		const result = await runCase(test_case, options);
		case_results.push(result);
		if (!result.ok && options.fail_fast) {
			break;
		}
	}

	const total = case_results.length;
	const passed = case_results.filter((result) => result.ok).length;
	const failed = total - passed;
	process.stdout.write(`\nSummary: total=${total} passed=${passed} failed=${failed}\n`);

	if (failed > 0) {
		for (const result of case_results.filter((item) => !item.ok)) {
			process.stdout.write(`  - ${result.name} (${result.file_path})\n`);
		}
		return 1;
	}

	return 0;
}

async function main(): Promise<void> {
	try {
		const exit_code = await run(process.argv.slice(2));
		process.exitCode = exit_code;
	} catch (error) {
		const message = error instanceof Error ? error.message : String(error);
		process.stderr.write(`${message}\n`);
		if (message.includes('--repo-root') || message.includes('No test case found') || message.includes('not found')) {
			process.stderr.write(`${usage}\n`);
		}
		process.exitCode = 1;
	}
}

function isDirectExecution(): boolean {
	const entry_path = process.argv[1];
	if (!entry_path) {
		return false;
	}
	return import.meta.url === pathToFileURL(entry_path).href;
}

if (isDirectExecution()) {
	void main();
}

export { run };
