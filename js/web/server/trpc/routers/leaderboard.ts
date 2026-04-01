import { and, count, desc, eq, lt } from 'drizzle-orm';
import z from 'zod';
import { db } from '~~/server/db/db';
import { createTRPCRouter, publicProcedure } from '~~/server/trpc/trpc';

import * as schema from '~~/server/db/schema';
import { env } from '~~/server/env';

export const leaderboardRouter = createTRPCRouter({
  get: publicProcedure
    .input(
      z.object({
        page: z.number().min(1).default(1),
        pageSize: z.number().min(1).max(100).default(10),
      }),
    )
    .query(async ({ input }) => {
      const leaderboard = await db.query.strategyGroup.findMany({
        orderBy: [desc(schema.strategyGroup.rating)],
        where: and(
          eq(schema.strategyGroup.status, 'normal'),
          lt(schema.strategyGroup.ratingDeviation, env.GLICKO2_LEADERBOARD_ENTRY_MAX_DEVIATION),
        ),
        columns: {
          id: true,
          name: true,
          rating: true,
          ratingDeviation: true,
        },
        with: {
          user: {
            columns: {
              name: true,
              image: true,
            },
          },
          ratingHistory: {
            orderBy: [desc(schema.ratingHistory.createdAt)],
            columns: {
              rating: true,
              createdAt: true,
            },
          },
        },
        limit: input.pageSize,
        offset: (input.page - 1) * input.pageSize,
      });

      return leaderboard;
    }),

  getTotal: publicProcedure
    .query(async () => {
      const [totalCount] = await db
        .select({ count: count() })
        .from(schema.strategyGroup)
        .where(eq(schema.strategyGroup.status, 'normal'));

      return totalCount?.count;
    }),
});
