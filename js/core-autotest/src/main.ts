#!/usr/bin/env node

import { access, mkdtemp, readFile, readdir, rm } from 'node:fs/promises';
import { constants as fs_constants, existsSync } from 'node:fs';
import { spawn } from 'node:child_process';
import { parseArgs } from 'node:util';
import { pathToFileURL } from 'node:url';
import path from 'node:path';
import os from 'node:os';
import process from 'node:process';
import { decodeReplay, type ReplayLogEntry, type ReplayLogSource, type ReplayLogType } from '@corertt/corereplay';

interface CliOptions {
	repo_root: string;
	autotest_dir: string;
	headless_path: string;
	case_name: string;
	fail_fast: boolean;
	verbose: boolean;
}

interface ProgramBinding {
	base: string;
	unit: string;
}

interface PayloadMatcher {
	equals?: string;
	regex?: string;
}

interface LogFilter {
	source?: number | string;
	type?: number | string;
	playerId?: number;
	unitId?: number;
	payload?: string | PayloadMatcher;
}

interface ExpectedLogAssertion {
	filter: LogFilter;
	atLeast?: number;
}

interface ForbiddenLogAssertion {
	filter: LogFilter;
	maxAllowed?: number;
}

interface CaseSpec {
	version: number;
	name: string;
	runs?: number;
	expectFailure?: boolean;
	timeLimitMs: number;
	maxTicks: number;
	seed?: string;
	aliases?: Record<string, string>;
	program: [ProgramBinding, ProgramBinding];
	expectedLogs?: ExpectedLogAssertion[];
	forbiddenLogs?: ForbiddenLogAssertion[];
}

interface TestCase {
	file_path: string;
	spec: CaseSpec;
}

interface FailureDetail {
	message: string;
	offending_log?: ReplayLogEntry;
}

interface RunResult {
	ok: boolean;
	failure?: FailureDetail;
	stderr_text: string;
}

interface CaseResult {
	ok: boolean;
	name: string;
	file_path: string;
	runs_total: number;
	runs_passed: number;
	failures: string[];
}

const usage = [
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

function parseCliArgs(argv: string[]): CliOptions {
	const parsed = parseArgs({
		args: argv,
		options: {
			'repo-root': { type: 'string', default: '' },
			'autotest-dir': { type: 'string', default: '' },
			'headless': { type: 'string', default: '' },
			'case': { type: 'string', default: '' },
			'fail-fast': { type: 'boolean', default: false },
			'verbose': { type: 'boolean', default: false },
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

function findRepoRoot(start_dir: string): string {
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

function pathExistsSync(file_path: string): boolean {
	return existsSync(file_path);
}

async function pathExists(file_path: string): Promise<boolean> {
	try {
		await access(file_path, fs_constants.F_OK);
		return true;
	} catch {
		return false;
	}
}

async function discoverCaseFiles(autotest_dir: string): Promise<string[]> {
	const entries = await readdir(autotest_dir, { withFileTypes: true });
	const json_files = entries
		.filter((entry) => entry.isFile() && entry.name.endsWith('.json'))
		.map((entry) => path.join(autotest_dir, entry.name))
		.sort((a, b) => a.localeCompare(b));
	return json_files;
}

function assertObject(value: unknown, path_name: string): asserts value is Record<string, unknown> {
	if (value == null || typeof value !== 'object' || Array.isArray(value)) {
		throw new Error(`${path_name} must be an object`);
	}
}

function asPositiveInt(value: unknown, path_name: string): number {
	if (!Number.isInteger(value) || Number(value) <= 0) {
		throw new Error(`${path_name} must be a positive integer`);
	}
	return Number(value);
}

function asNonNegativeInt(value: unknown, path_name: string): number {
	if (!Number.isInteger(value) || Number(value) < 0) {
		throw new Error(`${path_name} must be a non-negative integer`);
	}
	return Number(value);
}

function parseCaseSpec(raw_data: unknown, file_path: string): CaseSpec {
	assertObject(raw_data, `${file_path}`);

	const version = asPositiveInt(raw_data.version, `${file_path}.version`);
	if (version !== 1) {
		throw new Error(`${file_path}.version must be 1`);
	}

	if (typeof raw_data.name !== 'string' || raw_data.name.length === 0) {
		throw new Error(`${file_path}.name must be a non-empty string`);
	}

	const runs = raw_data.runs == null ? 1 : asPositiveInt(raw_data.runs, `${file_path}.runs`);
	if (raw_data.expectFailure != null && typeof raw_data.expectFailure !== 'boolean') {
		throw new Error(`${file_path}.expectFailure must be a boolean`);
	}
	const expect_failure = raw_data.expectFailure ?? false;
	const time_limit_ms = asPositiveInt(raw_data.timeLimitMs, `${file_path}.timeLimitMs`);
	const max_ticks = asPositiveInt(raw_data.maxTicks, `${file_path}.maxTicks`);
	const seed = raw_data.seed == null ? undefined : String(raw_data.seed);

	if (!Array.isArray(raw_data.program) || raw_data.program.length !== 2) {
		throw new Error(`${file_path}.program must be an array with exactly 2 player bindings`);
	}

	const program: [ProgramBinding, ProgramBinding] = raw_data.program.map((binding_raw, index) => {
		assertObject(binding_raw, `${file_path}.program[${index}]`);
		if (typeof binding_raw.base !== 'string' || binding_raw.base.length === 0) {
			throw new Error(`${file_path}.program[${index}].base must be a non-empty string`);
		}
		if (typeof binding_raw.unit !== 'string' || binding_raw.unit.length === 0) {
			throw new Error(`${file_path}.program[${index}].unit must be a non-empty string`);
		}
		return {
			base: binding_raw.base,
			unit: binding_raw.unit,
		};
	}) as [ProgramBinding, ProgramBinding];

	const aliases = parseAliases(raw_data.aliases, `${file_path}.aliases`);
	const expected_logs = parseExpectedAssertions(raw_data.expectedLogs, `${file_path}.expectedLogs`);
	const forbidden_logs = parseForbiddenAssertions(raw_data.forbiddenLogs, `${file_path}.forbiddenLogs`);

	return {
		version,
		name: raw_data.name,
		runs,
		expectFailure: expect_failure,
		timeLimitMs: time_limit_ms,
		maxTicks: max_ticks,
		seed,
		aliases,
		program,
		expectedLogs: expected_logs,
		forbiddenLogs: forbidden_logs,
	};
}

function parseAliases(raw_value: unknown, path_name: string): Record<string, string> | undefined {
	if (raw_value == null) {
		return undefined;
	}
	assertObject(raw_value, path_name);
	const aliases: Record<string, string> = {};
	for (const [key, value] of Object.entries(raw_value)) {
		if (typeof value !== 'string' || value.length === 0) {
			throw new Error(`${path_name}.${key} must be a non-empty string`);
		}
		aliases[key] = value;
	}
	return aliases;
}

function parseLogFilter(raw_value: unknown, path_name: string): LogFilter {
	assertObject(raw_value, path_name);
	const filter: LogFilter = {};

	if (raw_value.source != null) {
		if (typeof raw_value.source !== 'string' && !Number.isInteger(raw_value.source)) {
			throw new Error(`${path_name}.source must be a string or integer`);
		}
		filter.source = raw_value.source as string | number;
	}
	if (raw_value.type != null) {
		if (typeof raw_value.type !== 'string' && !Number.isInteger(raw_value.type)) {
			throw new Error(`${path_name}.type must be a string or integer`);
		}
		filter.type = raw_value.type as string | number;
	}
	if (raw_value.playerId != null) {
		filter.playerId = asNonNegativeInt(raw_value.playerId, `${path_name}.playerId`);
	}
	if (raw_value.unitId != null) {
		filter.unitId = asNonNegativeInt(raw_value.unitId, `${path_name}.unitId`);
	}
	if (raw_value.payload != null) {
		if (typeof raw_value.payload === 'string') {
			filter.payload = raw_value.payload;
		} else {
			assertObject(raw_value.payload, `${path_name}.payload`);
			if (raw_value.payload.equals == null && raw_value.payload.regex == null) {
				throw new Error(`${path_name}.payload must contain equals or regex`);
			}
			if (raw_value.payload.equals != null && typeof raw_value.payload.equals !== 'string') {
				throw new Error(`${path_name}.payload.equals must be a string`);
			}
			if (raw_value.payload.regex != null && typeof raw_value.payload.regex !== 'string') {
				throw new Error(`${path_name}.payload.regex must be a string`);
			}
			filter.payload = {
				equals: raw_value.payload.equals as string | undefined,
				regex: raw_value.payload.regex as string | undefined,
			};
		}
	}

	return filter;
}

function parseExpectedAssertions(raw_value: unknown, path_name: string): ExpectedLogAssertion[] {
	if (raw_value == null) {
		return [];
	}
	if (!Array.isArray(raw_value)) {
		throw new Error(`${path_name} must be an array`);
	}
	return raw_value.map((item, index) => {
		assertObject(item, `${path_name}[${index}]`);
		return {
			filter: parseLogFilter(item.filter, `${path_name}[${index}].filter`),
			atLeast: item.atLeast == null ? 1 : asPositiveInt(item.atLeast, `${path_name}[${index}].atLeast`),
		};
	});
}

function parseForbiddenAssertions(raw_value: unknown, path_name: string): ForbiddenLogAssertion[] {
	if (raw_value == null) {
		return [];
	}
	if (!Array.isArray(raw_value)) {
		throw new Error(`${path_name} must be an array`);
	}
	return raw_value.map((item, index) => {
		assertObject(item, `${path_name}[${index}]`);
		return {
			filter: parseLogFilter(item.filter, `${path_name}[${index}].filter`),
			maxAllowed: item.maxAllowed == null ? 0 : asNonNegativeInt(item.maxAllowed, `${path_name}[${index}].maxAllowed`),
		};
	});
}

async function loadCases(autotest_dir: string, case_name: string): Promise<TestCase[]> {
	const files = await discoverCaseFiles(autotest_dir);
	const cases: TestCase[] = [];
	for (const file_path of files) {
		const text = await readFile(file_path, 'utf8');
		const raw_data = JSON.parse(text) as unknown;
		const spec = parseCaseSpec(raw_data, file_path);
		if (case_name.length > 0 && spec.name !== case_name) {
			continue;
		}
		cases.push({ file_path, spec });
	}
	return cases;
}

function getDefaultAliases(): Record<string, string> {
	return {
		dummy: 'corelib/test/dummy.elf',
	};
}

async function resolveProgramRef(ref_value: string, spec: CaseSpec, repo_root: string): Promise<string> {
	if (ref_value.startsWith('@')) {
		const alias_name = ref_value.slice(1);
		if (alias_name.length === 0) {
			throw new Error('Alias reference cannot be empty');
		}
		const aliases = {
			...getDefaultAliases(),
			...(spec.aliases ?? {}),
		};
		const target = aliases[alias_name];
		if (!target) {
			throw new Error(`Unknown alias: ${ref_value}`);
		}
		const resolved = path.resolve(repo_root, target);
		if (!(await pathExists(resolved))) {
			throw new Error(`Alias ${ref_value} resolved to missing file: ${resolved}`);
		}
		return resolved;
	}

	if (ref_value.endsWith('.elf') || ref_value.includes('/') || ref_value.includes('\\')) {
		const resolved = path.isAbsolute(ref_value)
			? ref_value
			: path.resolve(repo_root, ref_value);
		if (!(await pathExists(resolved))) {
			throw new Error(`Program file not found: ${resolved}`);
		}
		return resolved;
	}

	const candidates = [
		path.join(repo_root, 'corelib', 'autotest', `${ref_value}.elf`),
		path.join(repo_root, 'corelib', 'test', `${ref_value}.elf`),
		path.join(repo_root, 'corelib', 'example', `${ref_value}.elf`),
	];
	for (const candidate of candidates) {
		if (await pathExists(candidate)) {
			return candidate;
		}
	}

	throw new Error(`Failed to resolve program reference: ${ref_value}`);
}

function parseSource(value: number | string): ReplayLogSource {
	if (typeof value === 'number') {
		if (value === 0 || value === 1) {
			return value;
		}
		throw new Error(`Invalid log source value: ${value}`);
	}

	const normalized = value.toLowerCase();
	if (normalized === 'system' || normalized === 'sys') {
		return 0;
	}
	if (normalized === 'player' || normalized === 'usr' || normalized === 'user') {
		return 1;
	}
	throw new Error(`Invalid log source string: ${value}`);
}

function parseType(value: number | string): ReplayLogType {
	if (typeof value === 'number') {
		if (value >= 0 && value <= 4) {
			return value as ReplayLogType;
		}
		throw new Error(`Invalid log type value: ${value}`);
	}

	const normalized = value.toLowerCase();
	if (normalized === 'custom') {
		return 0;
	}
	if (normalized === 'unitcreation' || normalized === 'unit_creation') {
		return 1;
	}
	if (normalized === 'unitdestruction' || normalized === 'unit_destruction') {
		return 2;
	}
	if (normalized === 'executionexception' || normalized === 'execution_exception') {
		return 3;
	}
	if (normalized === 'basecaptured' || normalized === 'base_captured') {
		return 4;
	}
	throw new Error(`Invalid log type string: ${value}`);
}

function decodePayload(payload: Uint8Array): string {
	return Buffer.from(payload).toString('latin1');
}

function matchesPayload(payload_text: string, matcher: string | PayloadMatcher): boolean {
	if (typeof matcher === 'string') {
		return payload_text === matcher;
	}

	if (matcher.equals != null && payload_text !== matcher.equals) {
		return false;
	}
	if (matcher.regex != null) {
		const regex = new RegExp(matcher.regex);
		if (!regex.test(payload_text)) {
			return false;
		}
	}
	return true;
}

function matchesFilter(log_entry: ReplayLogEntry, filter: LogFilter): boolean {
	if (filter.source != null && log_entry.source !== parseSource(filter.source)) {
		return false;
	}
	if (filter.type != null && log_entry.type !== parseType(filter.type)) {
		return false;
	}
	if (filter.playerId != null && log_entry.playerId !== filter.playerId) {
		return false;
	}
	if (filter.unitId != null && log_entry.unitId !== filter.unitId) {
		return false;
	}
	if (filter.payload != null && !matchesPayload(decodePayload(log_entry.payload), filter.payload)) {
		return false;
	}
	return true;
}

function sourceToString(source: ReplayLogSource): string {
	return source === 0 ? 'System' : 'Player';
}

function typeToString(type: ReplayLogType): string {
	switch (type) {
		case 0:
			return 'Custom';
		case 1:
			return 'UnitCreation';
		case 2:
			return 'UnitDestruction';
		case 3:
			return 'ExecutionException';
		case 4:
			return 'BaseCaptured';
		default:
			return 'Unknown';
	}
}

function formatLog(log_entry: ReplayLogEntry): string {
	return `tick=${log_entry.tick} player=${log_entry.playerId} unit=${log_entry.unitId} source=${sourceToString(log_entry.source)} type=${typeToString(log_entry.type)} payload=${JSON.stringify(decodePayload(log_entry.payload))}`;
}

function evaluateAssertions(log_entries: ReplayLogEntry[], spec: CaseSpec): FailureDetail | undefined {
	for (const [index, assertion] of (spec.expectedLogs ?? []).entries()) {
		const count = log_entries.filter((entry) => matchesFilter(entry, assertion.filter)).length;
		const expected_min = assertion.atLeast ?? 1;
		if (count < expected_min) {
			return {
				message: `expectedLogs[${index}] matched ${count}, expected at least ${expected_min}`,
			};
		}
	}

	for (const [index, assertion] of (spec.forbiddenLogs ?? []).entries()) {
		const matched = log_entries.filter((entry) => matchesFilter(entry, assertion.filter));
		const max_allowed = assertion.maxAllowed ?? 0;
		if (matched.length > max_allowed) {
			return {
				message: `forbiddenLogs[${index}] matched ${matched.length}, max allowed ${max_allowed}`,
				offending_log: matched[0],
			};
		}
	}

	return undefined;
}

async function runHeadlessOnce(
	spec: CaseSpec,
	repo_root: string,
	headless_path: string,
	verbose: boolean,
): Promise<RunResult> {
	const p1_base = await resolveProgramRef(spec.program[0].base, spec, repo_root);
	const p1_unit = await resolveProgramRef(spec.program[0].unit, spec, repo_root);
	const p2_base = await resolveProgramRef(spec.program[1].base, spec, repo_root);
	const p2_unit = await resolveProgramRef(spec.program[1].unit, spec, repo_root);

	const temp_dir = await mkdtemp(path.join(os.tmpdir(), 'corertt-autotest-'));
	const replay_file_path = path.join(temp_dir, 'replay.rtt');

	const args = [
		'--p1-base', p1_base,
		'--p1-unit', p1_unit,
		'--p2-base', p2_base,
		'--p2-unit', p2_unit,
		'--replay-file', replay_file_path,
		'--max-ticks', String(spec.maxTicks),
	];
	if (spec.seed != null && spec.seed.length > 0) {
		args.push('--seed', spec.seed);
	}

	if (verbose) {
		process.stdout.write(`cmd: ${headless_path} ${args.map((value) => JSON.stringify(value)).join(' ')}\n`);
	}

	try {
		const child = spawn(headless_path, args, {
			cwd: repo_root,
			stdio: ['ignore', 'ignore', 'pipe'],
		});

		let stderr_text = '';
		let timed_out = false;

		child.stderr.on('data', (chunk: Buffer) => {
			stderr_text += chunk.toString('utf8');
		});

		const timeout_handle = setTimeout(() => {
			timed_out = true;
			child.kill('SIGKILL');
		}, spec.timeLimitMs);

		const close_result = await new Promise<{ code: number | null; signal: NodeJS.Signals | null }>((resolve, reject) => {
			child.once('error', reject);
			child.once('close', (code, signal) => {
				resolve({ code, signal });
			});
		});

		clearTimeout(timeout_handle);

		if (timed_out) {
			return {
				ok: false,
				stderr_text,
				failure: {
					message: `headless timeout after ${spec.timeLimitMs}ms`,
				},
			};
		}
		if (close_result.signal != null) {
			return {
				ok: false,
				stderr_text,
				failure: {
					message: `headless terminated by signal ${close_result.signal}`,
				},
			};
		}
		if (close_result.code !== 0) {
			return {
				ok: false,
				stderr_text,
				failure: {
					message: `headless exited with code ${String(close_result.code)}`,
				},
			};
		}

		const replay_bytes = await readFile(replay_file_path);
		if (replay_bytes.length === 0) {
			return {
				ok: false,
				stderr_text,
				failure: {
					message: 'headless produced empty replay output',
				},
			};
		}

		let replay_data;
		try {
			replay_data = decodeReplay(replay_bytes);
		} catch (error) {
			const message = error instanceof Error ? error.message : String(error);
			return {
				ok: false,
				stderr_text,
				failure: {
					message: `failed to decode replay: ${message}`,
				},
			};
		}

		const all_logs = replay_data.ticks.flatMap((tick) => tick.logs);
		const assertion_failure = evaluateAssertions(all_logs, spec);
		if (assertion_failure != null) {
			return {
				ok: false,
				stderr_text,
				failure: assertion_failure,
			};
		}

		return {
			ok: true,
			stderr_text,
		};
	} finally {
		await rm(temp_dir, { recursive: true, force: true });
	}
}

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
