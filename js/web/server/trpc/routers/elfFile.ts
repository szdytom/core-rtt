import { TRPCError } from '@trpc/server';
import z from 'zod';
import { db } from '~~/server/db/db';
import { makeId } from '~~/server/lib/makeId';
import { getUploadUrl } from '~~/server/lib/s3';
import { createTRPCRouter, rateLimitedProtectedProcedure } from '~~/server/trpc/trpc';
import * as schema from '~~/server/db/schema';

export const elfFileRouter = createTRPCRouter({
  create: rateLimitedProtectedProcedure
    .input(z.object({
      fileName: z.string().optional(),
    }))
    .mutation(async ({ input, ctx }) => {
      const s3FileId = makeId(32);

      const url = await getUploadUrl(s3FileId);
      if (!url)
        throw new TRPCError({ code: 'INTERNAL_SERVER_ERROR', message: 'Failed to generate upload URL' });

      const [inserted] = await db.insert(schema.elfFile).values({
        userId: ctx.authSession.user.id,
        s3FileId,
        filename: input.fileName,
      }).returning();

      if (!inserted || !inserted.id) {
        throw new TRPCError({ code: 'INTERNAL_SERVER_ERROR', message: 'Failed to create ELF file record' });
      }

      return { id: inserted.id, url };
    }),
});
