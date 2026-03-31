import { describe, expect, test } from 'vitest';
import { TaskScheduler } from '../src/scheduler.js';

function defer(): { promise: Promise<void>; resolve: () => void; reject: (error: unknown) => void } {
	let resolve!: () => void;
	let reject!: (error: unknown) => void;
	const promise = new Promise<void>((res, rej) => {
		resolve = res;
		reject = rej;
	});
	return { promise, resolve, reject };
}

async function flushMicrotasks(): Promise<void> {
	await Promise.resolve();
	await Promise.resolve();
}

describe('TaskScheduler', () => {
	test('enforces concurrency and backfills when a task completes', async () => {
		const gates = new Map<string, { resolve: () => void }>();
		const started: string[] = [];
		const scheduler = new TaskScheduler<number>(2, async (task) => {
			started.push(task.matchId);
			const gate = defer();
			gates.set(task.matchId, { resolve: gate.resolve });
			await gate.promise;
		});

		scheduler.push('123456789001', 100);
		scheduler.push('123456789002', 200);
		scheduler.push('123456789003', 300);
		await flushMicrotasks();

		expect(started).toEqual(['123456789001', '123456789002']);
		expect(scheduler.canAcceptMore()).toBe(false);

		gates.get('123456789002')?.resolve();
		await flushMicrotasks();
		expect(started).toEqual(['123456789001', '123456789002', '123456789003']);

		gates.get('123456789001')?.resolve();
		gates.get('123456789003')?.resolve();
		await flushMicrotasks();
		expect(scheduler.snapshot().runningCount).toBe(0);
		expect(scheduler.snapshot().queuedCount).toBe(0);
		expect(scheduler.canAcceptMore()).toBe(true);
	});

	test('reports unfinished ids from both queued and running tasks', async () => {
		const gate = defer();
		const scheduler = new TaskScheduler<string>(1, async () => {
			await gate.promise;
		});

		scheduler.push('123456789011', 'a');
		scheduler.push('123456789022', 'b');
		scheduler.push('123456789033', 'c');
		await flushMicrotasks();

		const unfinished = scheduler.getUnfinishedMatchIds();
		expect(new Set(unfinished)).toEqual(new Set(['123456789011', '123456789022', '123456789033']));

		gate.resolve();
		await flushMicrotasks();
	});

	test('fires onIdle when the scheduler drains', async () => {
		let idleCount = 0;
		let resolveIdle!: () => void;
		const idlePromise = new Promise<void>((resolve) => {
			resolveIdle = resolve;
		});
		const scheduler = new TaskScheduler<number>(1, async () => {
			await flushMicrotasks();
		});
		scheduler.onIdle = () => {
			idleCount += 1;
			resolveIdle();
		};

		scheduler.push('123456789001', 1);
		scheduler.push('123456789002', 2);
		await idlePromise;

		expect(idleCount).toBe(1);
	});

	test('recovers from rejected handler and continues scheduling', async () => {
		const finished: string[] = [];
		const taskErrors: string[] = [];
		const scheduler = new TaskScheduler<number>(1, async (task) => {
			if (task.matchId === '123456789001') {
				throw new Error('boom');
			}
			finished.push(task.matchId);
		});
		scheduler.onTaskError = (task) => {
			taskErrors.push(task.matchId);
		};

		scheduler.push('123456789001', 1);
		scheduler.push('123456789002', 2);
		await flushMicrotasks();
		await flushMicrotasks();

		expect(taskErrors).toEqual(['123456789001']);
		expect(finished).toEqual(['123456789002']);
		expect(scheduler.snapshot().runningCount).toBe(0);
		expect(scheduler.snapshot().queuedCount).toBe(0);
	});
});
