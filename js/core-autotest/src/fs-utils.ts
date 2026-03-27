import { access, readdir } from 'node:fs/promises';
import { constants as fs_constants, existsSync } from 'node:fs';
import path from 'node:path';

export function pathExistsSync(file_path: string): boolean {
	return existsSync(file_path);
}

export async function pathExists(file_path: string): Promise<boolean> {
	try {
		await access(file_path, fs_constants.F_OK);
		return true;
	} catch {
		return false;
	}
}

export async function discoverCaseFiles(autotest_dir: string): Promise<string[]> {
	const entries = await readdir(autotest_dir, { withFileTypes: true });
	return entries
		.filter((entry) => entry.isFile() && entry.name.endsWith('.json'))
		.map((entry) => path.join(autotest_dir, entry.name))
		.sort((a, b) => a.localeCompare(b));
}

export async function discoverCaseFilesFromDirs(autotest_dirs: string[]): Promise<string[]> {
	const all_files = await Promise.all(autotest_dirs.map((dir_path) => discoverCaseFiles(dir_path)));
	return all_files.flat().sort((a, b) => a.localeCompare(b));
}
