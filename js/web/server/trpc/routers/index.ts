import { leaderboardRouter } from '~~/server/trpc/routers/leaderboard';
import { matchRouter } from '~~/server/trpc/routers/match';
import { strategiesRouter } from '~~/server/trpc/routers/strategies';
import { strategyGroupRouter } from '~~/server/trpc/routers/strategyGroup';
import { workerRouter } from '~~/server/trpc/routers/worker';
import { createTRPCRouter } from '~~/server/trpc/trpc';

export const appRouter = createTRPCRouter({
  strategies: strategiesRouter,
  strategyGroup: strategyGroupRouter,
  leaderboard: leaderboardRouter,
  match: matchRouter,
  worker: workerRouter,
});

export type AppRouter = typeof appRouter;
