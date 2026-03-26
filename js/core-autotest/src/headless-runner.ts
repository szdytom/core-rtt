import { mkdtemp, readFile, rm } from 'node:fs/promises';
import { spawn } from 'node:child_process';
import path from 'node:path';
import os from 'node:os';
import process from 'node:process';
import { decodeReplay } from '@corertt/corereplay';
import type { CaseSpec, RunResult } from './types.js';
import { resolveProgramRef } from './program-resolver.js';
import { evaluateAssertions } from './assertions.js';

export async function runHeadlessOnce(
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
