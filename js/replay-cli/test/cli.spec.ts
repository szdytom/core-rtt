import { afterEach, describe, expect, test } from 'vitest';
import { mkdtemp, readFile, rm, writeFile } from 'node:fs/promises';
import { join } from 'node:path';
import { tmpdir } from 'node:os';
import { run } from '../src/main.js';
import { sampleReplayBytesWithPayload, u8 } from './helpers.js';

const created_dirs: string[] = [];

afterEach(async () => {
	for (const dir of created_dirs.splice(0, created_dirs.length)) {
		await rm(dir, { recursive: true, force: true });
	}
});

async function createTempDir(): Promise<string> {
	const dir = await mkdtemp(join(tmpdir(), 'replay-cli-test-'));
	created_dirs.push(dir);
	return dir;
}

describe('replay-cli', () => {
	test('writes jsonl output to file', async () => {
		const dir = await createTempDir();
		const replay_path = join(dir, 'sample.replay');
		const output_path = join(dir, 'logs.jsonl');

		await writeFile(replay_path, sampleReplayBytesWithPayload(u8(0x41, 0x42, 0x43)));
		await run(['--replay-file', replay_path, '--output', output_path, '--format', 'jsonl']);

		const content = await readFile(output_path, 'utf8');
		expect(content).toBe('{"payload_hex":"414243","payload_text":"ABC","player_id":1,"source":"USR","tick":5,"type":"custom","unit_id":7}\n');
	});

	test('writes text output to file', async () => {
		const dir = await createTempDir();
		const replay_path = join(dir, 'sample.replay');
		const output_path = join(dir, 'logs.txt');

		await writeFile(replay_path, sampleReplayBytesWithPayload(u8(0x41, 0x42, 0x43)));
		await run(['--replay-file', replay_path, '--output', output_path, '--format', 'text']);

		const content = await readFile(output_path, 'utf8');
		expect(content).toBe('[0005 P1-07 USR] ABC\n');
	});

	test('formats multiline and escaped payload bytes in text output', async () => {
		const dir = await createTempDir();
		const replay_path = join(dir, 'sample.replay');
		const output_path = join(dir, 'logs.txt');
		const payload = u8(0x41, 0x0a, 0x09, 0x0d, 0x01, 0x42);

		await writeFile(replay_path, sampleReplayBytesWithPayload(payload));
		await run(['--replay-file', replay_path, '--output', output_path, '--format', 'text']);

		const content = await readFile(output_path, 'utf8');
		expect(content).toBe('[0005 P1-07 USR] A\n                 \t\r\\x01B\n');
	});

	test('writes to stdout when output is omitted', async () => {
		const dir = await createTempDir();
		const replay_path = join(dir, 'sample.replay');
		await writeFile(replay_path, sampleReplayBytesWithPayload(u8(0x41, 0x42, 0x43)));

		let captured = '';
		const original_write = process.stdout.write.bind(process.stdout);
		const replacement = ((chunk: string | Uint8Array) => {
			captured += typeof chunk === 'string' ? chunk : Buffer.from(chunk).toString('utf8');
			return true;
		}) as typeof process.stdout.write;
		process.stdout.write = replacement;
		try {
			await run(['--replay-file', replay_path, '--format', 'text']);
		} finally {
			process.stdout.write = original_write;
		}

		expect(captured).toBe('[0005 P1-07 USR] ABC\n');
	});

	test('throws on invalid format', async () => {
		const dir = await createTempDir();
		const replay_path = join(dir, 'sample.replay');
		await writeFile(replay_path, sampleReplayBytesWithPayload(u8(0x41)));

		await expect(run(['--replay-file', replay_path, '--format', 'yaml'])).rejects.toThrow('--format must be one of: jsonl, text');
	});

	test('throws when replay-file is missing', async () => {
		await expect(run(['--format', 'jsonl'])).rejects.toThrow('Missing required argument: --replay-file');
	});

	test('defaults to non-strict decode for truncated replay tail', async () => {
		const dir = await createTempDir();
		const replay_path = join(dir, 'sample.replay');
		const output_path = join(dir, 'logs.jsonl');
		const full = sampleReplayBytesWithPayload(u8(0x41, 0x42, 0x43));
		const truncated = full.subarray(0, full.length - 3);

		await writeFile(replay_path, truncated);
		await run(['--replay-file', replay_path, '--output', output_path, '--format', 'jsonl']);

		const content = await readFile(output_path, 'utf8');
		expect(content).toBe('{"payload_hex":"414243","payload_text":"ABC","player_id":1,"source":"USR","tick":5,"type":"custom","unit_id":7}\n');
	});

	test('fails on truncated replay when --strict is set', async () => {
		const dir = await createTempDir();
		const replay_path = join(dir, 'sample.replay');
		const full = sampleReplayBytesWithPayload(u8(0x41, 0x42, 0x43));
		const truncated = full.subarray(0, full.length - 3);

		await writeFile(replay_path, truncated);
		await expect(run(['--replay-file', replay_path, '--strict'])).rejects.toThrow('missing end marker');
	});
});
