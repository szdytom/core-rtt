import path from 'node:path';
import { spawn } from 'node:child_process';
import type { CaseSpec } from './types.js';
import { pathExists } from './fs-utils.js';

interface CargoArtifactMessage {
	reason?: string;
	executable?: string | null;
	target?: {
		name?: string;
		crate_types?: string[];
	};
}

type ProgramNamespace = 'c' | 'rusty';

const rust_example_cache = new Map<string, Promise<Map<string, string[]>>>();

export function parseCargoExampleExecutables(output_text: string): Map<string, string[]> {
	const executables = new Map<string, string[]>();
	const lines = output_text
		.split('\n')
		.map((line_text) => line_text.trim())
		.filter((line_text) => line_text.length > 0);

	for (const line_text of lines) {
		let parsed: CargoArtifactMessage;
		try {
			parsed = JSON.parse(line_text) as CargoArtifactMessage;
		} catch {
			continue;
		}

		if (parsed.reason !== 'compiler-artifact' || parsed.executable == null || parsed.target?.name == null) {
			continue;
		}
		if (!Array.isArray(parsed.target.crate_types) || !parsed.target.crate_types.includes('bin')) {
			continue;
		}

		const name = parsed.target.name;
		const list = executables.get(name) ?? [];
		list.push(parsed.executable);
		executables.set(name, list);
	}

	return executables;
}

async function collectRustExampleExecutables(repo_root: string): Promise<Map<string, string[]>> {
	const cached = rust_example_cache.get(repo_root);
	if (cached != null) {
		return cached;
	}

	const rust_root = path.join(repo_root, 'rusty-corelib');
	const collector = new Promise<Map<string, string[]>>((resolve, reject) => {
		const child = spawn(
			'cargo',
			['build', '--release', '--examples', '--message-format=json'],
			{
				cwd: rust_root,
				stdio: ['ignore', 'pipe', 'pipe'],
			},
		);

		let stdout_text = '';
		let stderr_text = '';

		child.stdout.on('data', (chunk: Buffer) => {
			stdout_text += chunk.toString('utf8');
		});
		child.stderr.on('data', (chunk: Buffer) => {
			stderr_text += chunk.toString('utf8');
		});

		child.once('error', (error) => {
			reject(new Error(`Failed to launch cargo: ${error.message}`));
		});
		child.once('close', (code) => {
			if (code !== 0) {
				reject(new Error(`cargo build failed with code ${String(code)}: ${stderr_text.trim()}`));
				return;
			}

			resolve(parseCargoExampleExecutables(stdout_text));
		});
	});

	rust_example_cache.set(repo_root, collector);
	try {
		return await collector;
	} catch (error) {
		rust_example_cache.delete(repo_root);
		throw error;
	}
}

function parseNamespacedRef(ref_value: string): { namespace: ProgramNamespace; name: string } | undefined {
	if (ref_value.startsWith('c::')) {
		const name = ref_value.slice(3);
		if (name.length === 0) {
			throw new Error('Program reference c:: must include a non-empty symbol name');
		}
		return { namespace: 'c', name };
	}

	if (ref_value.startsWith('rusty::')) {
		const name = ref_value.slice('rusty::'.length);
		if (name.length === 0) {
			throw new Error('Program reference rusty:: must include a non-empty symbol name');
		}
		return { namespace: 'rusty', name };
	}

	return undefined;
}

function expectAtMostOneMatch(matches: string[], original_ref: string): string | undefined {
	if (matches.length <= 1) {
		return matches[0];
	}

	throw new Error(
		`Ambiguous program reference ${original_ref}: found multiple matches:\n${matches.map((item) => `  - ${item}`).join('\n')}`,
	);
}

async function resolveCorelibCandidate(name: string, repo_root: string, original_ref: string): Promise<string | undefined> {
	const candidates = [
		path.join(repo_root, 'corelib', 'autotest', `${name}.elf`),
		path.join(repo_root, 'corelib', 'test', `${name}.elf`),
		path.join(repo_root, 'corelib', 'example', `${name}.elf`),
	];

	const matched: string[] = [];
	for (const candidate of candidates) {
		if (await pathExists(candidate)) {
			matched.push(candidate);
		}
	}

	return expectAtMostOneMatch(matched, original_ref);
}

async function resolveRustExampleExecutable(name: string, repo_root: string, original_ref: string): Promise<string | undefined> {
	const examples = await collectRustExampleExecutables(repo_root);
	const executables = examples.get(name);
	if (executables == null || executables.length === 0) {
		return undefined;
	}

	const resolved_matches: string[] = [];
	for (const executable of executables) {
		const resolved = path.isAbsolute(executable)
			? executable
			: path.resolve(repo_root, executable);
		if (!(await pathExists(resolved))) {
			throw new Error(`Rust example executable resolved to missing file: ${resolved}`);
		}
		resolved_matches.push(resolved);
	}

	return expectAtMostOneMatch(resolved_matches, original_ref);
}

export function getDefaultAliases(): Record<string, string> {
	return {
		dummy: 'corelib/example/dummy.elf',
	};
}

export async function resolveProgramRef(ref_value: string, spec: CaseSpec, repo_root: string): Promise<string> {
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

	const namespaced = parseNamespacedRef(ref_value);
	if (namespaced == null) {
		throw new Error(`Program reference must use c:: or rusty:: prefix (or @alias/path): ${ref_value}`);
	}

	if (namespaced.namespace === 'c') {
		const resolved = await resolveCorelibCandidate(namespaced.name, repo_root, ref_value);
		if (resolved != null) {
			return resolved;
		}
		throw new Error(`Failed to resolve c program reference: ${ref_value}`);
	}

	const rust_example = await resolveRustExampleExecutable(namespaced.name, repo_root, ref_value);
	if (rust_example != null) {
		return rust_example;
	}

	throw new Error(`Failed to resolve rusty program reference: ${ref_value}`);
}
