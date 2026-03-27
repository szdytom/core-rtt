import { spawn } from 'node:child_process';
import process from 'node:process';
import { decodeReplayStream } from '@corertt/corereplay';
import type { CaseSpec, RunResult } from './types.js';
import { resolveProgramRef } from './program-resolver.js';
import { evaluateAssertions } from './assertions.js';

async function decodeReplayLogsFromStdout(stdout_stream: AsyncIterable<Uint8Array>): Promise<import('@corertt/corereplay').ReplayLogEntry[]> {
	let saw_header = false;
	let saw_end_marker = false;
	const all_logs: import('@corertt/corereplay').ReplayLogEntry[] = [];

	for await (const event of decodeReplayStream(stdout_stream)) {
		if (event.kind === 'header') {
			saw_header = true;
			continue;
		}
		if (event.kind === 'tick') {
			all_logs.push(...event.tick.logs);
			continue;
		}
		saw_end_marker = true;
	}

	if (!saw_header) {
		throw new Error('headless produced empty replay output');
	}
	if (!saw_end_marker) {
		throw new Error('headless replay stream ended without end marker');
	}

	return all_logs;
}

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

	const args = [
		'--p1-base', p1_base,
		'--p1-unit', p1_unit,
		'--p2-base', p2_base,
		'--p2-unit', p2_unit,
		'--max-ticks', String(spec.maxTicks),
	];
	if (spec.seed != null && spec.seed.length > 0) {
		args.push('--seed', spec.seed);
	}

	if (verbose) {
		process.stdout.write(`cmd: ${headless_path} ${args.map((value) => JSON.stringify(value)).join(' ')}\n`);
	}

	const child = spawn(headless_path, args, {
		cwd: repo_root,
		stdio: ['ignore', 'pipe', 'pipe'],
	});

	let stderr_text = '';
	let timed_out = false;

	child.stderr.on('data', (chunk: Buffer) => {
		stderr_text += chunk.toString('utf8');
	});

	if (child.stdout == null) {
		return {
			ok: false,
			stderr_text,
			failure: {
				message: 'failed to capture replay stream from headless stdout',
			},
		};
	}

	const decode_result_promise = decodeReplayLogsFromStdout(child.stdout)
		.then((all_logs) => ({ ok: true as const, all_logs }))
		.catch((error: unknown) => ({
			ok: false as const,
			message: error instanceof Error ? error.message : String(error),
		}));

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

	const decode_result = await decode_result_promise;
	if (!decode_result.ok) {
		return {
			ok: false,
			stderr_text,
			failure: {
				message: `failed to decode replay stream: ${decode_result.message}`,
			},
		};
	}

	const assertion_failure = evaluateAssertions(decode_result.all_logs, spec);
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
}
