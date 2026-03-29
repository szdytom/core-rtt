import { access, mkdir, readFile, rename, rm, writeFile } from 'node:fs/promises';
import { constants as fsConstants, existsSync } from 'node:fs';

export async function pathExists(pathValue: string): Promise<boolean> {
	try {
		await access(pathValue, fsConstants.F_OK);
		return true;
	} catch {
		return false;
	}
}

export function pathExistsSync(pathValue: string): boolean {
	return existsSync(pathValue);
}

export async function ensureDir(pathValue: string): Promise<void> {
	await mkdir(pathValue, { recursive: true });
}

export async function writeFileAtomic(pathValue: string, content: string | Uint8Array): Promise<void> {
	const tempPath = `${pathValue}.tmp-${process.pid}-${Date.now()}`;
	await writeFile(tempPath, content);
	await rename(tempPath, pathValue);
}

export async function readJsonFile<T>(pathValue: string): Promise<T | null> {
	try {
		const content = await readFile(pathValue, 'utf8');
		return JSON.parse(content) as T;
	} catch {
		return null;
	}
}

export async function removePath(pathValue: string): Promise<void> {
	await rm(pathValue, { force: true, recursive: true });
}
