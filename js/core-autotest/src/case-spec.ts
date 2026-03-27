import { readFile } from 'node:fs/promises';
import type {
	CaseSpec,
	ExpectedLogAssertion,
	ForbiddenLogAssertion,
	LogFilter,
	TestCase,
} from './types.js';
import { discoverCaseFilesFromDirs } from './fs-utils.js';

function assertObject(value: unknown, path_name: string): asserts value is Record<string, unknown> {
	if (value == null || typeof value !== 'object' || Array.isArray(value)) {
		throw new Error(`${path_name} must be an object`);
	}
}

function asPositiveInt(value: unknown, path_name: string): number {
	if (!Number.isInteger(value) || Number(value) <= 0) {
		throw new Error(`${path_name} must be a positive integer`);
	}
	return Number(value);
}

function asNonNegativeInt(value: unknown, path_name: string): number {
	if (!Number.isInteger(value) || Number(value) < 0) {
		throw new Error(`${path_name} must be a non-negative integer`);
	}
	return Number(value);
}

function parseAliases(raw_value: unknown, path_name: string): Record<string, string> | undefined {
	if (raw_value == null) {
		return undefined;
	}
	assertObject(raw_value, path_name);
	const aliases: Record<string, string> = {};
	for (const [key, value] of Object.entries(raw_value)) {
		if (typeof value !== 'string' || value.length === 0) {
			throw new Error(`${path_name}.${key} must be a non-empty string`);
		}
		aliases[key] = value;
	}
	return aliases;
}

function parseLogFilter(raw_value: unknown, path_name: string): LogFilter {
	assertObject(raw_value, path_name);
	const filter: LogFilter = {};

	if (raw_value.source != null) {
		if (typeof raw_value.source !== 'string' && !Number.isInteger(raw_value.source)) {
			throw new Error(`${path_name}.source must be a string or integer`);
		}
		filter.source = raw_value.source as string | number;
	}
	if (raw_value.type != null) {
		if (typeof raw_value.type !== 'string' && !Number.isInteger(raw_value.type)) {
			throw new Error(`${path_name}.type must be a string or integer`);
		}
		filter.type = raw_value.type as string | number;
	}
	if (raw_value.playerId != null) {
		filter.playerId = asNonNegativeInt(raw_value.playerId, `${path_name}.playerId`);
	}
	if (raw_value.unitId != null) {
		filter.unitId = asNonNegativeInt(raw_value.unitId, `${path_name}.unitId`);
	}
	if (raw_value.payload != null) {
		if (typeof raw_value.payload === 'string') {
			filter.payload = raw_value.payload;
		} else {
			assertObject(raw_value.payload, `${path_name}.payload`);
			if (raw_value.payload.equals == null && raw_value.payload.regex == null) {
				throw new Error(`${path_name}.payload must contain equals or regex`);
			}
			if (raw_value.payload.equals != null && typeof raw_value.payload.equals !== 'string') {
				throw new Error(`${path_name}.payload.equals must be a string`);
			}
			if (raw_value.payload.regex != null && typeof raw_value.payload.regex !== 'string') {
				throw new Error(`${path_name}.payload.regex must be a string`);
			}
			filter.payload = {
				equals: raw_value.payload.equals as string | undefined,
				regex: raw_value.payload.regex as string | undefined,
			};
		}
	}

	return filter;
}

function parseExpectedAssertions(raw_value: unknown, path_name: string): ExpectedLogAssertion[] {
	if (raw_value == null) {
		return [];
	}
	if (!Array.isArray(raw_value)) {
		throw new Error(`${path_name} must be an array`);
	}
	return raw_value.map((item, index) => {
		assertObject(item, `${path_name}[${index}]`);
		return {
			filter: parseLogFilter(item.filter, `${path_name}[${index}].filter`),
			atLeast: item.atLeast == null ? 1 : asPositiveInt(item.atLeast, `${path_name}[${index}].atLeast`),
		};
	});
}

function parseForbiddenAssertions(raw_value: unknown, path_name: string): ForbiddenLogAssertion[] {
	if (raw_value == null) {
		return [];
	}
	if (!Array.isArray(raw_value)) {
		throw new Error(`${path_name} must be an array`);
	}
	return raw_value.map((item, index) => {
		assertObject(item, `${path_name}[${index}]`);
		return {
			filter: parseLogFilter(item.filter, `${path_name}[${index}].filter`),
			maxAllowed: item.maxAllowed == null ? 0 : asNonNegativeInt(item.maxAllowed, `${path_name}[${index}].maxAllowed`),
		};
	});
}

export function parseCaseSpec(raw_data: unknown, file_path: string): CaseSpec {
	assertObject(raw_data, `${file_path}`);

	const version = asPositiveInt(raw_data.version, `${file_path}.version`);
	if (version !== 1) {
		throw new Error(`${file_path}.version must be 1`);
	}

	if (typeof raw_data.name !== 'string' || raw_data.name.length === 0) {
		throw new Error(`${file_path}.name must be a non-empty string`);
	}

	const runs = raw_data.runs == null ? 1 : asPositiveInt(raw_data.runs, `${file_path}.runs`);
	if (raw_data.expectFailure != null && typeof raw_data.expectFailure !== 'boolean') {
		throw new Error(`${file_path}.expectFailure must be a boolean`);
	}
	const expect_failure = raw_data.expectFailure ?? false;
	const time_limit_ms = asPositiveInt(raw_data.timeLimitMs, `${file_path}.timeLimitMs`);
	const max_ticks = asPositiveInt(raw_data.maxTicks, `${file_path}.maxTicks`);
	const seed = raw_data.seed == null ? undefined : String(raw_data.seed);

	if (!Array.isArray(raw_data.program) || raw_data.program.length !== 2) {
		throw new Error(`${file_path}.program must be an array with exactly 2 player bindings`);
	}

	const program = raw_data.program.map((binding_raw, index) => {
		assertObject(binding_raw, `${file_path}.program[${index}]`);
		if (typeof binding_raw.base !== 'string' || binding_raw.base.length === 0) {
			throw new Error(`${file_path}.program[${index}].base must be a non-empty string`);
		}
		if (typeof binding_raw.unit !== 'string' || binding_raw.unit.length === 0) {
			throw new Error(`${file_path}.program[${index}].unit must be a non-empty string`);
		}
		return {
			base: binding_raw.base,
			unit: binding_raw.unit,
		};
	}) as CaseSpec['program'];

	const aliases = parseAliases(raw_data.aliases, `${file_path}.aliases`);
	const expected_logs = parseExpectedAssertions(raw_data.expectedLogs, `${file_path}.expectedLogs`);
	const forbidden_logs = parseForbiddenAssertions(raw_data.forbiddenLogs, `${file_path}.forbiddenLogs`);

	return {
		version,
		name: raw_data.name,
		runs,
		expectFailure: expect_failure,
		timeLimitMs: time_limit_ms,
		maxTicks: max_ticks,
		seed,
		aliases,
		program,
		expectedLogs: expected_logs,
		forbiddenLogs: forbidden_logs,
	};
}

export async function loadCases(autotest_dirs: string[], case_name: string): Promise<TestCase[]> {
	const files = await discoverCaseFilesFromDirs(autotest_dirs);
	const cases: TestCase[] = [];
	for (const file_path of files) {
		const text = await readFile(file_path, 'utf8');
		const raw_data = JSON.parse(text) as unknown;
		const spec = parseCaseSpec(raw_data, file_path);
		if (case_name.length > 0 && spec.name !== case_name) {
			continue;
		}
		cases.push({ file_path, spec });
	}
	return cases;
}
