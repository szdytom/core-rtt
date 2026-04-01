import { and, count, eq, ne, or } from 'drizzle-orm';
import { db } from '~~/server/db/db';
import { createTRPCRouter, protectedProcedure, rateLimitedProtectedProcedure } from '~~/server/trpc/trpc';
import * as schema from '~~/server/db/schema';
import z from 'zod';
import { TRPCError } from '@trpc/server';
import { deleteFile, getUploadUrl } from '~~/server/lib/s3';
import { makeId } from '~~/server/lib/makeId';

export const strategiesRouter = createTRPCRouter({
  listMine: protectedProcedure.query(async ({ ctx }) => {
    const strategies = await db.query.strategy.findMany({
      where: eq(schema.strategy.userId, ctx.authSession.user.id),
      with: {
        elfFile: true,
      },
    });

    return strategies;
  }),

  create: rateLimitedProtectedProcedure
    .input(z.object({
      name: z.string(),
      type: z.enum(['base', 'unit']),
      fileName: z.string(),
    }))
    .mutation(async ({ input, ctx }) => {
      return await db.transaction(async (tx) => {
        const [existingStrategies] = await tx
          .select({ count: count() })
          .from(schema.strategy)
          .where(eq(schema.strategy.userId, ctx.authSession.user.id));
        if (!existingStrategies)
          throw new TRPCError({ code: 'INTERNAL_SERVER_ERROR', message: 'Failed to count existing strategies.' });

        const userLimit = await tx.query.user.findFirst({
          where: eq(schema.user.id, ctx.authSession.user.id),
          columns: { strategyLimit: true },
        });
        if (!userLimit)
          throw new TRPCError({ code: 'INTERNAL_SERVER_ERROR', message: 'User not found.' });
          // Check if the user has reached the limit of strategies they can create
        if (existingStrategies.count >= userLimit.strategyLimit)
          throw new TRPCError({ code: 'BAD_REQUEST', message: 'You have reached the limit of strategies you can create.' });

        const [insertedStrategy] = await tx.insert(schema.strategy).values({
          name: input.name,
          type: input.type,
          userId: ctx.authSession.user.id,
        }).returning();

        if (!insertedStrategy || !insertedStrategy.id)
          throw new TRPCError({ code: 'INTERNAL_SERVER_ERROR', message: 'Failed to create strategy.' });

        // Create an ELF file record for the strategy
        const s3FileId = makeId(32);
        const uploadUrl = await getUploadUrl(s3FileId);
        if (!uploadUrl)
          throw new TRPCError({ code: 'INTERNAL_SERVER_ERROR', message: 'Failed to generate upload URL' });

        const [insertedElfFile] = await tx.insert(schema.elfFile).values({
          userId: ctx.authSession.user.id,
          s3FileId,
          filename: input.fileName,
          strategyId: insertedStrategy.id,
        }).returning();

        if (!insertedElfFile || !insertedElfFile.id)
          throw new TRPCError({ code: 'INTERNAL_SERVER_ERROR', message: 'Failed to create ELF file record' });

        return { uploadUrl };
      }, {
        behavior: 'immediate',
      });
    }),

  delete: protectedProcedure
    .input(z.object({
      id: z.string(),
    }))
    .mutation(async ({ input, ctx }) => {
      const strategy = await db.query.strategy.findFirst({
        where: eq(schema.strategy.id, input.id),
        columns: { userId: true },
        with: {
          elfFile: {
            columns: { s3FileId: true, id: true },
          },
        },
      });

      if (!strategy || strategy.userId !== ctx.authSession.user.id)
        throw new TRPCError({ code: 'NOT_FOUND', message: 'Strategy not found' });

      const relatedStrategyGroups = await db.query.strategyGroup.findFirst({
        where: and(
          or(
            eq(schema.strategyGroup.strategyBaseId, input.id),
            eq(schema.strategyGroup.strategyUnitId, input.id),
          ),
          ne(schema.strategyGroup.status, 'deleted'),
        ),
        columns: { id: true },
      });

      if (relatedStrategyGroups)
        throw new TRPCError({ code: 'BAD_REQUEST', message: 'Cannot delete strategy that is part of a strategy group. Please remove it from the group first.' });

      await db.transaction(async (tx) => {
        // If the strategy has an associated ELF file, delete it from S3 and the database
        if (strategy.elfFile) {
          await deleteFile(strategy.elfFile.s3FileId);
          await tx.delete(schema.elfFile).where(eq(schema.elfFile.id, strategy.elfFile.id));
        }

        // Delete the strategy
        await tx.delete(schema.strategy).where(eq(schema.strategy.id, input.id));
      });
    }),
});
