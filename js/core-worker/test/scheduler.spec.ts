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
		const gates = new Map<number, { resolve: () => void }>();
		const started: number[] = [];
		const scheduler = new TaskScheduler<number>(2, async (task) => {
			started.push(task.matchId);
			const gate = defer();
			gates.set(task.matchId, { resolve: gate.resolve });
			await gate.promise;
		});

		scheduler.push(1, 100);
		scheduler.push(2, 200);
		scheduler.push(3, 300);
		await flushMicrotasks();

		expect(started).toEqual([1, 2]);
		expect(scheduler.canAcceptMore()).toBe(false);

		gates.get(2)?.resolve();
		await flushMicrotasks();
		expect(started).toEqual([1, 2, 3]);

		gates.get(1)?.resolve();
		gates.get(3)?.resolve();
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

		scheduler.push(11, 'a');
		scheduler.push(22, 'b');
		scheduler.push(33, 'c');
		await flushMicrotasks();

		const unfinished = scheduler.getUnfinishedMatchIds();
		expect(new Set(unfinished)).toEqual(new Set([11, 22, 33]));

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

		scheduler.push(1, 1);
		scheduler.push(2, 2);
		await idlePromise;

		expect(idleCount).toBe(1);
	});

	test('recovers from rejected handler and continues scheduling', async () => {
		const finished: number[] = [];
		const taskErrors: number[] = [];
		const scheduler = new TaskScheduler<number>(1, async (task) => {
			if (task.matchId === 1) {
				throw new Error('boom');
			}
			finished.push(task.matchId);
		});
		scheduler.onTaskError = (task) => {
			taskErrors.push(task.matchId);
		};

		scheduler.push(1, 1);
		scheduler.push(2, 2);
		await flushMicrotasks();
		await flushMicrotasks();

		expect(taskErrors).toEqual([1]);
		expect(finished).toEqual([2]);
		expect(scheduler.snapshot().runningCount).toBe(0);
		expect(scheduler.snapshot().queuedCount).toBe(0);
	});
});
