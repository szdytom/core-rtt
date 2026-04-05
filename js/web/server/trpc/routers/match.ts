import z from 'zod';
import { db } from '~~/server/db/db';
import { createTRPCRouter, protectedProcedure, rateLimitedProtectedProcedure } from '~~/server/trpc/trpc';
import * as schema from '~~/server/db/schema';
import { eq } from 'drizzle-orm';
import { TRPCError } from '@trpc/server';
import { makeId } from '~~/server/lib/makeId';
import { getDownloadUrl, getUploadUrl } from '~~/server/lib/s3';
import { workerTaskBroker } from '~~/server/lib/workerTaskBroker';
import { StrategyGroupDescriptor, TaskAssignPacket } from '@corertt/worker-codec';

interface StrategyDescriptorSeed {
  strategyGroupId: string;
  baseLastModified: Date;
  unitLastModified: Date;
  baseS3FileId: string;
  unitS3FileId: string;
}

async function buildStrategyDescriptor(seed: StrategyDescriptorSeed): Promise<StrategyGroupDescriptor> {
  const [baseStrategyUrl, unitStrategyUrl] = await Promise.all([
    getDownloadUrl(seed.baseS3FileId),
    getDownloadUrl(seed.unitS3FileId),
  ]);

  const descriptor = new StrategyGroupDescriptor();
  descriptor.strategyGroupId = seed.strategyGroupId;
  descriptor.baseLastModified = seed.baseLastModified;
  descriptor.unitLastModified = seed.unitLastModified;
  descriptor.baseStrategyUrl = baseStrategyUrl;
  descriptor.unitStrategyUrl = unitStrategyUrl;
  return descriptor;
}

export const matchRouter = createTRPCRouter({
  get: protectedProcedure
    .input(z.object({ matchId: z.string() }))
    .query(async ({ input, ctx }) => {
      const match = await db.query.match.findFirst({
        where: eq(schema.match.id, input.matchId),
        with: {
          strategyGroupA: true,
          strategyGroupB: true,
        },
      });

      if (!match) {
        throw new TRPCError({ code: 'NOT_FOUND', message: 'Match not found' });
      }

      // Ensure the authenticated user is a participant in the match
      if (match.strategyGroupA.userId !== ctx.authSession.user.id && match.strategyGroupB.userId !== ctx.authSession.user.id) {
        throw new TRPCError({ code: 'FORBIDDEN', message: 'You do not have access to this match' });
      }

      return match;
    }),

  challenge: rateLimitedProtectedProcedure
    .input(
      z.object({
        myStrategyGroupId: z.string()
          .min(1, 'Your strategy group ID is required')
          .max(255, 'Strategy group ID must be less than 255 characters'),
        opponentStrategyGroupId: z.string()
          .min(1, 'Opponent strategy group ID is required')
          .max(255, 'Opponent strategy group ID must be less than 255 characters'),
      }),
    )
    .mutation(async ({ input, ctx }) => {
      const queuedMatch = await db.transaction(async (tx) => {
        const myStrategyGroup = await tx.query.strategyGroup.findFirst({
          where: eq(schema.strategyGroup.id, input.myStrategyGroupId),
          columns: { id: true, userId: true },
          with: {
            strategyBase: {
              columns: { updatedAt: true },
              with: {
                elfFile: {
                  columns: { s3FileId: true },
                },
              },
            },
            strategyUnit: {
              columns: { updatedAt: true },
              with: {
                elfFile: {
                  columns: { s3FileId: true },
                },
              },
            },
          },
        });

        // Ensure the strategy group belongs to the authenticated user
        if (!myStrategyGroup || myStrategyGroup.userId !== ctx.authSession.user.id)
          throw new TRPCError({ code: 'NOT_FOUND', message: 'Strategy group not found' });

        if (myStrategyGroup.strategyBase.elfFile == null || myStrategyGroup.strategyUnit.elfFile == null)
          throw new TRPCError({ code: 'BAD_REQUEST', message: 'Your strategy group is missing uploaded strategy binaries' });

        const opponentStrategyGroup = await tx.query.strategyGroup.findFirst({
          where: eq(schema.strategyGroup.id, input.opponentStrategyGroupId),
          columns: { id: true },
          with: {
            strategyBase: {
              columns: { updatedAt: true },
              with: {
                elfFile: {
                  columns: { s3FileId: true },
                },
              },
            },
            strategyUnit: {
              columns: { updatedAt: true },
              with: {
                elfFile: {
                  columns: { s3FileId: true },
                },
              },
            },
          },
        });

        if (!opponentStrategyGroup)
          throw new TRPCError({ code: 'NOT_FOUND', message: 'Opponent strategy group not found' });

        if (opponentStrategyGroup.strategyBase.elfFile == null || opponentStrategyGroup.strategyUnit.elfFile == null)
          throw new TRPCError({ code: 'BAD_REQUEST', message: 'Opponent strategy group is missing uploaded strategy binaries' });

        // Create a new match with status 'pending'
        const [insertedMatch] = await tx.insert(schema.match).values({
          strategyGroupAId: input.myStrategyGroupId,
          strategyGroupBId: input.opponentStrategyGroupId,
          status: 'pending',
        }).returning();

        if (!insertedMatch)
          throw new TRPCError({ code: 'INTERNAL_SERVER_ERROR', message: 'Failed to create match' });

        const replayS3FileId = makeId(32);

        await tx.insert(schema.replayFile).values({
          s3FileId: replayS3FileId,
          matchId: insertedMatch.id,
        });

        return {
          matchId: insertedMatch.id,
          replayS3FileId,
          strategies: [
            {
              strategyGroupId: myStrategyGroup.id,
              baseLastModified: myStrategyGroup.strategyBase.updatedAt,
              unitLastModified: myStrategyGroup.strategyUnit.updatedAt,
              baseS3FileId: myStrategyGroup.strategyBase.elfFile.s3FileId,
              unitS3FileId: myStrategyGroup.strategyUnit.elfFile.s3FileId,
            },
            {
              strategyGroupId: opponentStrategyGroup.id,
              baseLastModified: opponentStrategyGroup.strategyBase.updatedAt,
              unitLastModified: opponentStrategyGroup.strategyUnit.updatedAt,
              baseS3FileId: opponentStrategyGroup.strategyBase.elfFile.s3FileId,
              unitS3FileId: opponentStrategyGroup.strategyUnit.elfFile.s3FileId,
            },
          ] as [StrategyDescriptorSeed, StrategyDescriptorSeed],
        };
      }, {
        behavior: 'immediate',
      });

      try {
        const [strategyA, strategyB, replayUploadUrl] = await Promise.all([
          buildStrategyDescriptor(queuedMatch.strategies[0]),
          buildStrategyDescriptor(queuedMatch.strategies[1]),
          getUploadUrl(queuedMatch.replayS3FileId),
        ]);

        const taskPacket = new TaskAssignPacket();
        taskPacket.matchId = queuedMatch.matchId;
        // TODO: populate mapData with actual map data if needed in the future
        taskPacket.mapData = new ArrayBuffer(0);
        taskPacket.strategies = [strategyA, strategyB];
        taskPacket.replayUploadUrl = replayUploadUrl;

        workerTaskBroker.enqueueTask(taskPacket);
      } catch {
        await db.update(schema.match)
          .set({ status: 'rejected' })
          .where(eq(schema.match.id, queuedMatch.matchId));

        throw new TRPCError({ code: 'INTERNAL_SERVER_ERROR', message: 'Failed to queue match for worker' });
      }

      return { matchId: queuedMatch.matchId };
    }),
});
