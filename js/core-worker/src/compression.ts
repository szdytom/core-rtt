import { spawn } from 'node:child_process';

type ZstdModuleShape = {
	compress?: (input: Buffer | Uint8Array, level?: number) => Promise<Buffer | Uint8Array> | Buffer | Uint8Array;
	Zstd?: {
		compress?: (input: Buffer | Uint8Array, level?: number) => Promise<Buffer | Uint8Array> | Buffer | Uint8Array;
	};
};

async function compressWithNativeModule(data: Buffer): Promise<Buffer | null> {
	try {
		const moduleValue = await import('@mongodb-js/zstd') as unknown as ZstdModuleShape;
		const compressFn = moduleValue.compress ?? moduleValue.Zstd?.compress;
		if (compressFn == null) {
			return null;
		}
		const result = await compressFn(data);
		return Buffer.isBuffer(result) ? result : Buffer.from(result);
	} catch {
		return null;
	}
}

async function compressWithCli(data: Buffer): Promise<Buffer> {
	const child = spawn('zstd', ['-q', '-c'], {
		stdio: ['pipe', 'pipe', 'pipe'],
	});

	const stdoutChunks: Buffer[] = [];
	let stderrText = '';

	child.stdout.on('data', (chunk: Buffer) => {
		stdoutChunks.push(chunk);
	});
	child.stderr.on('data', (chunk: Buffer) => {
		stderrText += chunk.toString('utf8');
	});

	const finished = new Promise<void>((resolve, reject) => {
		child.once('error', reject);
		child.once('close', (code, signal) => {
			if (signal != null) {
				reject(new Error(`zstd CLI terminated by signal ${signal}`));
				return;
			}
			if (code !== 0) {
				reject(new Error(`zstd CLI exited with ${String(code)}: ${stderrText.trim()}`));
				return;
			}
			resolve();
		});
	});

	child.stdin.write(data);
	child.stdin.end();
	await finished;
	return Buffer.concat(stdoutChunks);
}

export async function compressZstd(data: Buffer): Promise<Buffer> {
	const nativeCompressed = await compressWithNativeModule(data);
	if (nativeCompressed != null) {
		return nativeCompressed;
	}
	return await compressWithCli(data);
}
