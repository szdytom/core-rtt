import { count, desc, eq } from 'drizzle-orm';
import z from 'zod';
import { db } from '~~/server/db/db';
import { createTRPCRouter, publicProcedure } from '~~/server/trpc/trpc';

import * as schema from '~~/server/db/schema';

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
        where: eq(schema.strategyGroup.status, 'normal'),
        columns: {
          id: true,
          name: true,
          rating: true,
        },
        with: {
          user: {
            columns: {
              name: true,
              image: true,
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
