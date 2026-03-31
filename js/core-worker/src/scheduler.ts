import type { SnowflakeId } from '@corertt/worker-codec';

export interface ScheduledTask<T> {
	matchId: SnowflakeId;
	payload: T;
	enqueuedAtMs: number;
}

export interface SchedulerSnapshot {
	runningCount: number;
	queuedCount: number;
	unfinishedMatchIds: SnowflakeId[];
}

export class TaskScheduler<T> {
	private readonly concurrency: number;
	private readonly queue: ScheduledTask<T>[] = [];
	private readonly runningTasks = new Map<SnowflakeId, ScheduledTask<T>>();
	private readonly taskHandler: (task: ScheduledTask<T>) => Promise<void>;

	public onTaskError?: (task: ScheduledTask<T>, error: unknown) => void;
	public onIdle?: () => void;

	public constructor(concurrency: number, taskHandler: (task: ScheduledTask<T>) => Promise<void>) {
		if (!Number.isInteger(concurrency) || concurrency <= 0) {
			throw new Error('concurrency must be a positive integer');
		}
		this.concurrency = concurrency;
		this.taskHandler = taskHandler;
	}

	public push(matchId: SnowflakeId, payload: T): void {
		this.queue.push({
			matchId,
			payload,
			enqueuedAtMs: Date.now(),
		});
		this.maybeStartTasks();
	}

	public canAcceptMore(): boolean {
		return this.runningTasks.size + this.queue.length < this.concurrency;
	}

	public getUnfinishedMatchIds(): SnowflakeId[] {
		const queuedIds = this.queue.map((task) => task.matchId);
		const runningIds = Array.from(this.runningTasks.keys());
		return [...queuedIds, ...runningIds];
	}

	public snapshot(): SchedulerSnapshot {
		return {
			runningCount: this.runningTasks.size,
			queuedCount: this.queue.length,
			unfinishedMatchIds: this.getUnfinishedMatchIds(),
		};
	}

	private maybeStartTasks(): void {
		while (this.runningTasks.size < this.concurrency && this.queue.length > 0) {
			const task = this.queue.shift();
			if (task == null) {
				return;
			}
			this.runningTasks.set(task.matchId, task);
			void this.taskHandler(task)
				.catch((error: unknown) => {
					this.onTaskError?.(task, error);
				})
				.finally(() => {
					this.runningTasks.delete(task.matchId);
					if (this.runningTasks.size === 0 && this.queue.length === 0) {
						this.onIdle?.();
					}
					this.maybeStartTasks();
				});
		}
	}
}
