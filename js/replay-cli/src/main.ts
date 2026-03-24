#!/usr/bin/env node

import { parseArgs } from 'node:util';
import { once } from 'node:events';
import { readFile, writeFile } from 'node:fs/promises';
import { pathToFileURL } from 'node:url';
import { decodeReplay } from '@corertt/corereplay';
import { formatReplayLogEntryLines, type OutputFormat, toReplayLogJsonLine } from './format.js';

interface CliOptions {
	replay_file: string;
	output: string;
	format: OutputFormat;
}

const usage = [
	'Usage: corertt-replay-log --replay-file <path> [--output <path>] [--format <jsonl|text>]',
	'',
	'Options:',
	'  --replay-file <path>    input replay file path',
	'  --output <path>         output file path, stdout when omitted',
	'  --format <value>        output format: jsonl or text (default: jsonl)',
].join('\n');

function parseCliArgs(argv: string[]): CliOptions {
	const parsed = parseArgs({
		args: argv,
		options: {
			'replay-file': {
				type: 'string',
			},
			output: {
				type: 'string',
				default: '',
			},
			format: {
				type: 'string',
				default: 'jsonl',
			},
			help: {
				type: 'boolean',
				short: 'h',
			},
		},
		allowPositionals: false,
		strict: true,
	});

	if (parsed.values.help) {
		process.stdout.write(`${usage}\n`);
		process.exit(0);
	}

	const replay_file = parsed.values['replay-file'];
	if (typeof replay_file !== 'string' || replay_file.length === 0) {
		throw new Error('Missing required argument: --replay-file');
	}

	const output = parsed.values.output;
	if (typeof output !== 'string') {
		throw new Error('Invalid value for --output');
	}

	const format = parsed.values.format;
	if (format !== 'jsonl' && format !== 'text') {
		throw new Error('--format must be one of: jsonl, text');
	}

	return {
		replay_file,
		output,
		format,
	};
}

function buildOutputContent(replay_file_bytes: Uint8Array, format: OutputFormat): string {
	const replay_data = decodeReplay(replay_file_bytes);
	const lines: string[] = [];

	for (const tick of replay_data.ticks) {
		for (const entry of tick.logs) {
			if (format === 'text') {
				lines.push(...formatReplayLogEntryLines(entry));
				continue;
			}

			lines.push(JSON.stringify(toReplayLogJsonLine(entry)));
		}
	}

	if (lines.length === 0) {
		return '';
	}

	return `${lines.join('\n')}\n`;
}

async function writeToStdout(content: string): Promise<void> {
	if (content.length === 0) {
		return;
	}

	if (!process.stdout.write(content)) {
		await once(process.stdout, 'drain');
	}
}

export async function run(argv: string[]): Promise<void> {
	const options = parseCliArgs(argv);
	const replay_file_bytes = await readFile(options.replay_file);
	const content = buildOutputContent(replay_file_bytes, options.format);

	if (options.output.length === 0) {
		await writeToStdout(content);
		return;
	}

	await writeFile(options.output, content, { encoding: 'utf8' });
}

async function main(): Promise<void> {
	try {
		await run(process.argv.slice(2));
	} catch (error) {
		const message = error instanceof Error ? error.message : String(error);
		process.stderr.write(`${message}\n`);

		if (!message.includes('Usage:') && message.includes('--replay-file')) {
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
