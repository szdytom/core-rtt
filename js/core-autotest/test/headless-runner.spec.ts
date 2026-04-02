import path from 'node:path';
import { describe, expect, test } from 'vitest';
import { appendGameRuleArgs, resolveCaseMapPath } from '../src/headless-runner.js';
import type { CaseSpec } from '../src/types.js';

function createSpec(overrides: Partial<CaseSpec>): CaseSpec {
	return {
		version: 1,
		name: 'map resolver test',
		timeLimitMs: 100,
		maxTicks: 10,
		program: [
			{ base: '@dummy', unit: '@dummy' },
			{ base: '@dummy', unit: '@dummy' },
		],
		...overrides,
	};
}

describe('resolveCaseMapPath', () => {
	test('resolves map filename under corelib/autotest/map', async () => {
		const repoRoot = path.resolve(__dirname, '../../..');
		const spec = createSpec({ map: 'tiny.txt' });
		const resolved = await resolveCaseMapPath(spec, repoRoot);
		expect(resolved).toBe(path.join(repoRoot, 'corelib', 'autotest', 'map', 'tiny.txt'));
	});

	test('rejects path traversal and separators', async () => {
		const repoRoot = path.resolve(__dirname, '../../..');
		const slashSpec = createSpec({ map: 'foo/bar.txt' });
		await expect(resolveCaseMapPath(slashSpec, repoRoot)).rejects.toThrow('map must be a filename under corelib/autotest/map');

		const parentSpec = createSpec({ map: '..' });
		await expect(resolveCaseMapPath(parentSpec, repoRoot)).rejects.toThrow('map filename is invalid: ..');
	});

	test('throws when map file does not exist', async () => {
		const repoRoot = path.resolve(__dirname, '../../..');
		const spec = createSpec({ map: 'not-found-map.txt' });
		await expect(resolveCaseMapPath(spec, repoRoot)).rejects.toThrow('map file not found');
	});
});

describe('appendGameRuleArgs', () => {
	test('maps gameRules fields to headless CLI flags', () => {
		const args: string[] = [];
		appendGameRuleArgs(args, {
			width: 48,
			height: 32,
			baseSize: 4,
			unitHealth: 120,
			naturalEnergyRate: 2,
			resourceZoneEnergyRate: 20,
			attackCooldown: 6,
			captureTurnThreshold: 9,
			capacityLv1: 150,
			capacityLv2: 900,
			visionLv1: 4,
			visionLv2: 8,
			capacityUpgradeCost: 450,
			visionUpgradeCost: 1100,
			damageUpgradeCost: 650,
			manufactCost: 700,
		});

		expect(args).toEqual([
			'--width', '48',
			'--height', '32',
			'--base-size', '4',
			'--unit-health', '120',
			'--natural-energy-rate', '2',
			'--resource-zone-energy-rate', '20',
			'--attack-cooldown', '6',
			'--capture-turn-threshold', '9',
			'--capacity-lv1', '150',
			'--capacity-lv2', '900',
			'--vision-lv1', '4',
			'--vision-lv2', '8',
			'--capacity-upgrade-cost', '450',
			'--vision-upgrade-cost', '1100',
			'--damage-upgrade-cost', '650',
			'--manufact-cost', '700',
		]);
	});
});
