#!/usr/bin/env node

import { pathToFileURL } from 'node:url';
import process from 'node:process';
import { parseWorkerConfig, usage } from './config.js';
import { pathExists } from './fs-utils.js';
import { logger, refreshLoggerLevel } from './logger.js';
import { CoreWorker } from './worker.js';

async function run(): Promise<number> {
	const config = parseWorkerConfig();
	refreshLoggerLevel();
	if (!(await pathExists(config.headlessPath))) {
		throw new Error(`corertt_headless not found: ${config.headlessPath}`);
	}
	const worker = new CoreWorker(config);
	await worker.start();
	return await new Promise<number>((resolve) => {
		const stop = async () => {
			await worker.stop();
			resolve(0);
		};
		process.once('SIGINT', () => { void stop(); });
		process.once('SIGTERM', () => { void stop(); });
	});
}

async function main(): Promise<void> {
	try {
		if (process.argv.includes('--help') || process.argv.includes('-h')) {
			process.stdout.write(`${usage}\n`);
			process.exitCode = 0;
			return;
		}

		process.exitCode = await run();
	} catch (error) {
		refreshLoggerLevel();
		const message = error instanceof Error ? error.message : String(error);
		logger.error({ error: message }, 'worker startup failed');
		if (message.includes('missing required env keys') || message.includes('not found')) {
			logger.error({ usage }, 'worker usage');
		}
		process.exitCode = 1;
	}
}

function isDirectExecution(): boolean {
	const entryPath = process.argv[1];
	if (!entryPath) {
		return false;
	}
	return import.meta.url === pathToFileURL(entryPath).href;
}

if (isDirectExecution()) {
	void main();
}

export { run };
