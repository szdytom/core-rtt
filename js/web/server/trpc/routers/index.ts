import { elfFileRouter } from '~~/server/trpc/routers/elfFile';
import { leaderboardRouter } from '~~/server/trpc/routers/leaderboard';
import { strategiesRouter } from '~~/server/trpc/routers/strategies';
import { strategyGroupRouter } from '~~/server/trpc/routers/strategyGroup';
import { createTRPCRouter } from '~~/server/trpc/trpc';

export const appRouter = createTRPCRouter({
  strategies: strategiesRouter,
  strategyGroup: strategyGroupRouter,
  leaderboard: leaderboardRouter,
  elfFile: elfFileRouter,
});

export type AppRouter = typeof appRouter;
