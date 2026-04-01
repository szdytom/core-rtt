// Adapted from OrJDev/trpc-limiter (https://github.com/OrJDev/trpc-limiter/blob/main/packages/memory/src/store.ts)

/**
 * Calculates the time when all hit counters will be reset.
 **/
const calculateNextResetTime = (windowMs: number): Date => {
  const resetTime = new Date();
  resetTime.setMilliseconds(resetTime.getMilliseconds() + windowMs);
  return resetTime;
};

export class MemoryStore {
  /** The duration of time before which all hit counts are reset (in milliseconds). */
  private windowMs: number;

  /** The map that stores the number of hits for each client in memory. */
  private hits: Record<string, number | undefined>;

  /** The time at which all hit counts will be reset. */
  private resetTime: Date;

  /** Reference to the active timer. */
  private interval: NodeJS.Timeout;

  constructor(options: { windowMs: number }) {
    this.windowMs = options.windowMs;
    this.resetTime = calculateNextResetTime(this.windowMs);
    this.hits = {};

    // Reset hit counts for ALL clients every `windowMs` - this will also
    // re-calculate the `resetTime`
    this.interval = setInterval(async () => {
      await this.resetAll();
    }, this.windowMs);
    // Cleaning up the interval will be taken care of by the `shutdown` method.
    if (this.interval.unref) this.interval.unref();
  }

  async increment(key: string) {
    const totalHits = (this.hits[key] ?? 0) + 1;
    this.hits[key] = totalHits;

    return {
      totalHits,
      resetTime: this.resetTime,
    };
  }

  /** Reset everyone's hit counter. */
  private async resetAll(): Promise<void> {
    this.hits = {};
    this.resetTime = calculateNextResetTime(this.windowMs);
  }
}
