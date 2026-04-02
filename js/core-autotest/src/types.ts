import type { ReplayLogEntry } from '@corertt/corereplay';

export interface CliOptions {
	repo_root: string;
	autotest_dirs: string[];
	headless_path: string;
	case_name: string;
	fail_fast: boolean;
	verbose: boolean;
}

export interface ProgramBinding {
	base: string;
	unit: string;
}

export interface GameRulesConfig {
	width?: number;
	height?: number;
	baseSize?: number;
	unitHealth?: number;
	naturalEnergyRate?: number;
	resourceZoneEnergyRate?: number;
	attackCooldown?: number;
	captureTurnThreshold?: number;
	capacityLv1?: number;
	capacityLv2?: number;
	visionLv1?: number;
	visionLv2?: number;
	capacityUpgradeCost?: number;
	visionUpgradeCost?: number;
	damageUpgradeCost?: number;
	manufactCost?: number;
}

export interface PayloadMatcher {
	equals?: string;
	regex?: string;
}

export interface LogFilter {
	source?: number | string;
	type?: number | string;
	playerId?: number;
	unitId?: number;
	payload?: string | PayloadMatcher;
}

export interface ExpectedLogAssertion {
	filter: LogFilter;
	atLeast?: number;
}

export interface ForbiddenLogAssertion {
	filter: LogFilter;
	maxAllowed?: number;
}

export interface CaseSpec {
	version: number;
	name: string;
	runs?: number;
	expectFailure?: boolean;
	timeLimitMs: number;
	maxTicks: number;
	seed?: string;
	map?: string;
	aliases?: Record<string, string>;
	gameRules?: GameRulesConfig;
	program: [ProgramBinding, ProgramBinding];
	expectedLogs?: ExpectedLogAssertion[];
	forbiddenLogs?: ForbiddenLogAssertion[];
}

export interface TestCase {
	file_path: string;
	spec: CaseSpec;
}

export interface FailureDetail {
	message: string;
	offending_log?: ReplayLogEntry;
}

export interface RunResult {
	ok: boolean;
	failure?: FailureDetail;
	stderr_text: string;
}

export interface CaseResult {
	ok: boolean;
	name: string;
	file_path: string;
	runs_total: number;
	runs_passed: number;
	failures: string[];
}
