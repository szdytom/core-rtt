import { strategiesRouter } from '~~/server/trpc/routers/strategies';
import { strategyGroupRouter } from '~~/server/trpc/routers/strategyGroup';
import { createTRPCRouter } from '~~/server/trpc/trpc';

export const appRouter = createTRPCRouter({
  strategies: strategiesRouter,
  strategyGroup: strategyGroupRouter,
});

export type AppRouter = typeof appRouter;
