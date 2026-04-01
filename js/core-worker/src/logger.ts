import process from 'node:process';
import pino from 'pino';

const loggerLevel = process.env.CORERTT_WORKER_LOG_LEVEL ?? 'info';

export const logger = pino(
	{
		name: 'core-worker',
		level: loggerLevel,
		base: undefined,
	},
	pino.destination({ dest: 2, sync: false }),
);

export function refreshLoggerLevel(): void {
	logger.level = process.env.CORERTT_WORKER_LOG_LEVEL ?? 'info';
}
