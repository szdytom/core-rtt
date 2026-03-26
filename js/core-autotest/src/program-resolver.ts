import path from 'node:path';
import type { CaseSpec } from './types.js';
import { pathExists } from './fs-utils.js';

export function getDefaultAliases(): Record<string, string> {
	return {
		dummy: 'corelib/test/dummy.elf',
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
