import { eq } from 'drizzle-orm';
import { db } from '~~/server/db/db';
import { createTRPCRouter, protectedProcedure } from '~~/server/trpc/trpc';
import * as schema from '~~/server/db/schema';
import z from 'zod';
import { TRPCError } from '@trpc/server';
import { deleteFile } from '~~/server/lib/s3';

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

  create: protectedProcedure
    .input(z.object({
      name: z.string(),
      type: z.enum(['base', 'unit']),
      elfFileId: z.string(),
    }))
    .mutation(async ({ input, ctx }) => {
      // Fetch the ELF file to ensure it exists and belongs to the authenticated user
      // This can be outside the transaction since it's just a read operation, and doesn't introduce race conditions for the subsequent operations
      const elfFile = await db.query.elfFile.findFirst({
        where: eq(schema.elfFile.id, input.elfFileId),
      });

      // Check if the ELF file exists and belongs to the authenticated user
      if (!elfFile || elfFile.userId !== ctx.authSession.user.id)
        throw new TRPCError({ code: 'NOT_FOUND', message: 'ELF file not found' });

      try {
        await db.transaction(async (tx) => {
          const existingStrategies = await tx.query.strategy.findMany({
            where: eq(schema.strategy.userId, ctx.authSession.user.id),
          });

          const userLimit = await tx.query.user.findFirst({
            where: eq(schema.user.id, ctx.authSession.user.id),
            columns: { strategyLimit: true },
          });
          if (!userLimit)
            throw new TRPCError({ code: 'INTERNAL_SERVER_ERROR', message: 'User not found.' });
          // Check if the user has reached the limit of strategies they can create
          if (existingStrategies.length >= userLimit.strategyLimit)
            throw new TRPCError({ code: 'BAD_REQUEST', message: 'You have reached the limit of strategies you can create.' });

          const [strategy] = await tx.insert(schema.strategy).values({
            name: input.name,
            type: input.type,
            userId: ctx.authSession.user.id,
          }).returning();

          if (!strategy || !strategy.id) {
            throw new TRPCError({ code: 'INTERNAL_SERVER_ERROR', message: 'Failed to create strategy.' });
          }

          // Associate the ELF file with the newly created strategy
          await tx
            .update(schema.elfFile)
            .set({ strategyId: strategy.id })
            .where(eq(schema.elfFile.id, input.elfFileId));
        }, {
          behavior: 'immediate',
        });
      } catch (error) {
        // If any error occurs during the transaction, the strategy creation must have failed. Attempt to clean up the uploaded ELF file
        try {
          await db.transaction(async (tx) => {
            await deleteFile(elfFile.s3FileId);
            await tx.delete(schema.elfFile).where(eq(schema.elfFile.id, input.elfFileId));
          });
        } catch {
          // Don't throw cleanup errors. The user should see the original error message
        }

        throw error;
      }
    }),
});
