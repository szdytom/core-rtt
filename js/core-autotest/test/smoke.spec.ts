import { describe, expect, test } from 'vitest';
import { run } from '../src/main.js';

describe('core-autotest CLI', () => {
	test('returns non-zero for unknown case', async () => {
		const exit_code = await run(['--case', '__missing_case__']);
		expect(exit_code).toBe(1);
	});
});
