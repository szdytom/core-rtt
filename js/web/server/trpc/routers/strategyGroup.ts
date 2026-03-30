import { TRPCError } from '@trpc/server';
import { and, desc, eq, ne } from 'drizzle-orm';
import { db } from '~~/server/db/db';
import { createTRPCRouter, protectedProcedure } from '~~/server/trpc/trpc';
import * as schema from '~~/server/db/schema';
import z from 'zod';

export const strategyGroupRouter = createTRPCRouter({
  getLimit: protectedProcedure.query(async ({ ctx }) => {
    const user = await db.query.user.findFirst({
      where: eq(schema.user.id, ctx.authSession.user.id),
      columns: { strategyGroupLimit: true },
    });
    if (!user)
      throw new TRPCError({ code: 'INTERNAL_SERVER_ERROR', message: 'User not found.' });

    return user.strategyGroupLimit;
  }),

  listMine: protectedProcedure.query(async ({ ctx }) => {
    const strategyGroups = await db.query.strategyGroup.findMany({
      where: and(
        eq(schema.strategyGroup.userId, ctx.authSession.user.id),
        ne(schema.strategyGroup.status, 'deleted'),
      ),
      with: {
        strategyBase: true,
        strategyUnit: true,
      },
      orderBy: [desc(schema.strategyGroup.rating)],
    });

    return strategyGroups;
  }),

  create: protectedProcedure
    .input(
      z.object({
        name: z.string().min(1, 'Name is required').max(100, 'Name must be at most 100 characters'),
        strategyBaseId: z.string().min(1, 'Base strategy is required'),
        strategyUnitId: z.string().min(1, 'Unit strategy is required'),
      }),
    )
    .mutation(async ({ ctx, input }) => {
      return await db.transaction(async (tx) => {
        const strategyBase = await tx.query.strategy.findFirst({
          where: eq(schema.strategy.id, input.strategyBaseId),
        });
        const strategyUnit = await tx.query.strategy.findFirst({
          where: eq(schema.strategy.id, input.strategyUnitId),
        });

        if (!strategyBase || !strategyUnit)
          throw new TRPCError({ code: 'BAD_REQUEST', message: 'Base or unit strategy not found.' });

        if (strategyBase.type !== 'base' || strategyUnit.type !== 'unit')
          throw new TRPCError({ code: 'BAD_REQUEST', message: 'Selected strategy is not of the correct type.' });

        // Ensure the strategies belong to the authenticated user
        if (strategyBase.userId !== ctx.authSession.user.id || strategyUnit.userId !== ctx.authSession.user.id)
          throw new TRPCError({ code: 'UNAUTHORIZED', message: 'You do not own the selected strategies.' });

        const existingGroups = await tx.query.strategyGroup.findMany({
          where: and(
            eq(schema.strategyGroup.userId, ctx.authSession.user.id),
            ne(schema.strategyGroup.status, 'deleted'),
          ),
        });

        // Check if a strategy group with the same base and unit strategies already exists for the user
        if (existingGroups.some(group => group.strategyBaseId == input.strategyBaseId && group.strategyUnitId == input.strategyUnitId)) {
          throw new TRPCError({ code: 'BAD_REQUEST', message: 'Strategy group with the same base and unit strategies already exists.' });
        }

        const userLimit = await tx.query.user.findFirst({
          where: eq(schema.user.id, ctx.authSession.user.id),
          columns: { strategyGroupLimit: true },
        });
        if (!userLimit) {
          throw new TRPCError({ code: 'INTERNAL_SERVER_ERROR', message: 'User not found.' });
        }

        // Check if the user has reached the limit of active strategy groups
        if (existingGroups.filter(x => x.status === 'normal').length >= userLimit.strategyGroupLimit) {
          return await tx.insert(schema.strategyGroup).values({
            name: input.name,
            strategyBaseId: input.strategyBaseId,
            strategyUnitId: input.strategyUnitId,
            userId: ctx.authSession.user.id,
            status: 'disabled', // Set to disabled if over the limit
          }).returning();
        }

        return await tx.insert(schema.strategyGroup).values({
          name: input.name,
          strategyBaseId: input.strategyBaseId,
          strategyUnitId: input.strategyUnitId,
          userId: ctx.authSession.user.id,
          status: 'normal', // Set to normal if under the limit
        }).returning();
      }, {
        behavior: 'immediate',
      });
    }),

  toggleActive: protectedProcedure
    .input(
      z.object({
        id: z.string().min(1, 'Strategy group id is required'),
      }),
    )
    .mutation(async ({ ctx, input }) => {
      await db.transaction(async (tx) => {
        const normalGroups = await tx.query.strategyGroup.findMany({
          where: and(
            eq(schema.strategyGroup.userId, ctx.authSession.user.id),
            eq(schema.strategyGroup.status, 'normal'),
          ),
        });

        const strategyGroup = await tx.query.strategyGroup.findFirst({
          where: and(
            eq(schema.strategyGroup.id, input.id),
            // Ensure the strategy group belongs to the authenticated user
            eq(schema.strategyGroup.userId, ctx.authSession.user.id),
          ),
        });

        if (!strategyGroup)
          throw new TRPCError({ code: 'NOT_FOUND', message: 'Strategy group not found.' });

        if (strategyGroup.status === 'normal') {
          // If currently active, simply disable it
          await tx.update(schema.strategyGroup)
            .set({ status: 'disabled' })
            .where(eq(schema.strategyGroup.id, input.id));
        } else if (strategyGroup.status === 'disabled') {
          // If currently disabled, check if the user has reached the limit of active strategy groups
          const userLimit = await tx.query.user.findFirst({
            where: eq(schema.user.id, ctx.authSession.user.id),
            columns: { strategyGroupLimit: true },
          });
          if (!userLimit) {
            throw new TRPCError({ code: 'INTERNAL_SERVER_ERROR', message: 'User not found.' });
          }
          if (normalGroups.length >= userLimit.strategyGroupLimit) {
            throw new TRPCError({ code: 'BAD_REQUEST', message: `You can only have up to ${userLimit.strategyGroupLimit} active strategy groups.` });
          }

          await tx.update(schema.strategyGroup)
            .set({ status: 'normal' })
            .where(eq(schema.strategyGroup.id, input.id));
        } else {
          throw new TRPCError({ code: 'BAD_REQUEST', message: 'Strategy group is not in a toggleable state.' });
        }
      }, {
        behavior: 'immediate',
      });
    }),

  delete: protectedProcedure
    .input(
      z.object({
        id: z.string().min(1, 'Strategy group id is required'),
      }),
    )
    .mutation(async ({ ctx, input }) => {
      const strategyGroup = await db.query.strategyGroup.findFirst({
        where: and(
          eq(schema.strategyGroup.id, input.id),
          // Ensure the strategy group belongs to the authenticated user
          eq(schema.strategyGroup.userId, ctx.authSession.user.id),
        ),
      });

      if (!strategyGroup)
        throw new TRPCError({ code: 'NOT_FOUND', message: 'Strategy group not found.' });
      if (strategyGroup.status === 'banned')
        throw new TRPCError({ code: 'UNAUTHORIZED', message: 'You cannot change the status of this strategy group.' });

      await db.update(schema.strategyGroup)
        .set({ status: 'deleted' })
        .where(eq(schema.strategyGroup.id, input.id));
    }),
});
