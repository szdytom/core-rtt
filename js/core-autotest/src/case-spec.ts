import { readFile } from 'node:fs/promises';
import type {
	CaseSpec,
	GameRulesConfig,
	ExpectedLogAssertion,
	ForbiddenLogAssertion,
	LogFilter,
	TestCase,
} from './types.js';
import { discoverCaseFilesFromDirs } from './fs-utils.js';

const DEFAULT_GAME_RULES = {
	width: 64,
	height: 64,
	baseSize: 5,
	unitHealth: 100,
	naturalEnergyRate: 1,
	resourceZoneEnergyRate: 25,
	attackCooldown: 3,
	captureTurnThreshold: 8,
	capacityLv1: 200,
	capacityLv2: 1000,
	visionLv1: 5,
	visionLv2: 9,
	capacityUpgradeCost: 400,
	visionUpgradeCost: 1000,
	damageUpgradeCost: 600,
	manufactCost: 500,
} as const;

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

function parseRuleValue(
	rawValue: Record<string, unknown>,
	fieldName: keyof GameRulesConfig,
	pathName: string,
	minValue: number,
	maxValue: number,
): number | undefined {
	const value = rawValue[fieldName];
	if (value == null) {
		return undefined;
	}
	if (!Number.isInteger(value)) {
		throw new Error(`${pathName}.${String(fieldName)} must be an integer`);
	}
	const numericValue = Number(value);
	if (numericValue < minValue || numericValue > maxValue) {
		throw new Error(
			`${pathName}.${String(fieldName)} must be within [${minValue}, ${maxValue}]`
		);
	}
	return numericValue;
}

function parseGameRules(rawValue: unknown, pathName: string): GameRulesConfig | undefined {
	if (rawValue == null) {
		return undefined;
	}
	assertObject(rawValue, pathName);

	const gameRules: GameRulesConfig = {};

	const width = parseRuleValue(rawValue, 'width', pathName, 4, 255);
	if (width != null) {
		gameRules.width = width;
	}
	const height = parseRuleValue(rawValue, 'height', pathName, 4, 255);
	if (height != null) {
		gameRules.height = height;
	}
	const baseSize = parseRuleValue(rawValue, 'baseSize', pathName, 2, 8);
	if (baseSize != null) {
		gameRules.baseSize = baseSize;
	}
	const unitHealth = parseRuleValue(rawValue, 'unitHealth', pathName, 1, 255);
	if (unitHealth != null) {
		gameRules.unitHealth = unitHealth;
	}
	const naturalEnergyRate = parseRuleValue(
		rawValue,
		'naturalEnergyRate',
		pathName,
		1,
		255,
	);
	if (naturalEnergyRate != null) {
		gameRules.naturalEnergyRate = naturalEnergyRate;
	}
	const resourceZoneEnergyRate = parseRuleValue(
		rawValue,
		'resourceZoneEnergyRate',
		pathName,
		1,
		255,
	);
	if (resourceZoneEnergyRate != null) {
		gameRules.resourceZoneEnergyRate = resourceZoneEnergyRate;
	}
	const attackCooldown = parseRuleValue(rawValue, 'attackCooldown', pathName, 1, 255);
	if (attackCooldown != null) {
		gameRules.attackCooldown = attackCooldown;
	}
	const captureTurnThreshold = parseRuleValue(
		rawValue,
		'captureTurnThreshold',
		pathName,
		1,
		255,
	);
	if (captureTurnThreshold != null) {
		gameRules.captureTurnThreshold = captureTurnThreshold;
	}
	const capacityLv1 = parseRuleValue(rawValue, 'capacityLv1', pathName, 1, 65535);
	if (capacityLv1 != null) {
		gameRules.capacityLv1 = capacityLv1;
	}
	const capacityLv2 = parseRuleValue(rawValue, 'capacityLv2', pathName, 1, 65535);
	if (capacityLv2 != null) {
		gameRules.capacityLv2 = capacityLv2;
	}
	const visionLv1 = parseRuleValue(rawValue, 'visionLv1', pathName, 1, 255);
	if (visionLv1 != null) {
		gameRules.visionLv1 = visionLv1;
	}
	const visionLv2 = parseRuleValue(rawValue, 'visionLv2', pathName, 1, 255);
	if (visionLv2 != null) {
		gameRules.visionLv2 = visionLv2;
	}
	const capacityUpgradeCost = parseRuleValue(
		rawValue,
		'capacityUpgradeCost',
		pathName,
		1,
		65535,
	);
	if (capacityUpgradeCost != null) {
		gameRules.capacityUpgradeCost = capacityUpgradeCost;
	}
	const visionUpgradeCost = parseRuleValue(
		rawValue,
		'visionUpgradeCost',
		pathName,
		1,
		65535,
	);
	if (visionUpgradeCost != null) {
		gameRules.visionUpgradeCost = visionUpgradeCost;
	}
	const damageUpgradeCost = parseRuleValue(
		rawValue,
		'damageUpgradeCost',
		pathName,
		1,
		65535,
	);
	if (damageUpgradeCost != null) {
		gameRules.damageUpgradeCost = damageUpgradeCost;
	}
	const manufactCost = parseRuleValue(rawValue, 'manufactCost', pathName, 1, 65535);
	if (manufactCost != null) {
		gameRules.manufactCost = manufactCost;
	}

	const effectiveGameRules = {
		...DEFAULT_GAME_RULES,
		...gameRules,
	};
	if (effectiveGameRules.capacityLv2 < effectiveGameRules.capacityLv1) {
		throw new Error(`${pathName}.capacityLv2 must be >= ${pathName}.capacityLv1`);
	}
	if (effectiveGameRules.visionLv2 < effectiveGameRules.visionLv1) {
		throw new Error(`${pathName}.visionLv2 must be >= ${pathName}.visionLv1`);
	}

	return gameRules;
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
	const map = raw_data.map == null ? undefined : String(raw_data.map);

	if (map != null && map.length === 0) {
		throw new Error(`${file_path}.map must be a non-empty string`);
	}
	if (seed != null && seed.length > 0 && map != null) {
		throw new Error(`${file_path}.seed and ${file_path}.map are mutually exclusive`);
	}

	const gameRules = parseGameRules(raw_data.gameRules, `${file_path}.gameRules`);
	if (map != null && gameRules != null) {
		if (gameRules.width != null || gameRules.height != null || gameRules.baseSize != null) {
			throw new Error(
				`${file_path}.gameRules.width, ${file_path}.gameRules.height, and ${file_path}.gameRules.baseSize cannot be used with ${file_path}.map`
			);
		}
	}

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
		map,
		aliases,
		gameRules,
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
