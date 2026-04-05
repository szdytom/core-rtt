import z from 'zod';
import { db } from '~~/server/db/db';
import { adminProcedure, createTRPCRouter } from '~~/server/trpc/trpc';
import * as schema from '~~/server/db/schema';
import { makeId } from '~~/server/lib/makeId';
import crypto from 'crypto';
import { eq } from 'drizzle-orm';
import { TRPCError } from '@trpc/server';
import { workerTaskBroker } from '~~/server/lib/workerTaskBroker';

export const workerRouter = createTRPCRouter({
  list: adminProcedure.query(async () => {
    const workers = await db.query.worker.findMany({
      columns: {
        secret: false,
      },
      with: {
        creator: {
          columns: {
            id: true,
            name: true,
            image: true,
          },
        },
      },
    });

    return { workers };
  }),

  create: adminProcedure
    .input(z.object({
      name: z.string().min(1).max(100),
      description: z.string().max(1000).optional(),
    }))
    .mutation(async ({ input, ctx }) => {
      const secret = makeId(32);
      const salt = crypto.randomBytes(16);
      const hashedSecret = crypto.scryptSync(secret, salt, 64);

      const [insertedWorker] = await db.insert(schema.worker).values({
        name: input.name,
        description: input.description,
        creatorId: ctx.authSession.user.id,
        salt: salt.toString('hex'),
        secret: hashedSecret.toString('hex'),
      }).returning();

      if (!insertedWorker)
        throw new TRPCError({ code: 'INTERNAL_SERVER_ERROR', message: 'Failed to create worker' });

      return {
        workerId: insertedWorker.id,
        secret,
      };
    }),

  update: adminProcedure
    .input(z.object({
      id: z.string()
        .min(1, 'Worker id is required')
        .max(64, 'Worker ID must be at most 64 characters long'),
      name: z.string().min(1).max(100),
      description: z.string().max(1000).optional(),
      status: z.enum(['normal', 'disabled']),
    }))
    .mutation(async ({ input }) => {
      const worker = await db.query.worker.findFirst({
        where: eq(schema.worker.id, input.id),
        columns: {
          id: true,
        },
      });

      if (!worker)
        throw new TRPCError({ code: 'NOT_FOUND', message: 'Worker not found' });

      await db.update(schema.worker)
        .set({
          name: input.name,
          description: input.description,
          status: input.status,
        })
        .where(eq(schema.worker.id, input.id));

      workerTaskBroker.setWorkerEnabled(input.id, input.status === 'normal');
    }),

  delete: adminProcedure
    .input(z.object({
      id: z.string()
        .min(1, 'Worker id is required')
        .max(64, 'Worker ID must be at most 64 characters long'),
    }))
    .mutation(async ({ input }) => {
      const worker = await db.query.worker.findFirst({
        where: eq(schema.worker.id, input.id),
        columns: {
          id: true,
        },
      });

      if (!worker)
        throw new TRPCError({ code: 'NOT_FOUND', message: 'Worker not found' });

      workerTaskBroker.setWorkerEnabled(input.id, false);

      await db.delete(schema.worker).where(eq(schema.worker.id, input.id));
    }),
});
