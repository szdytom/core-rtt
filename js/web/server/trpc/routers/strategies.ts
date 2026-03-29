import { eq } from 'drizzle-orm';
import { db } from '~~/server/db/db';
import { createTRPCRouter, protectedProcedure } from '~~/server/trpc/trpc';
import * as schema from '~~/server/db/schema';

export const strategiesRouter = createTRPCRouter({
  list: protectedProcedure.query(async ({ ctx }) => {
    const strategies = await db.query.strategy.findMany({
      where: eq(schema.strategy.userId, ctx.authSession.user.id),
      with: {
        elfFile: true,
      },
    });

    return strategies;
  }),
});
