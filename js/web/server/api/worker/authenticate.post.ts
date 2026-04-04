import { eq } from 'drizzle-orm';
import z from 'zod';
import { db } from '~~/server/db/db';
import * as schema from '~~/server/db/schema';
import crypto from 'crypto';
import { produceAccessToken } from '~~/server/lib/jwt';

const workerAuthenticateSchema = z.object({
  worker_id: z.string()
    .min(1, 'Worker id is required')
    .max(64, 'Worker ID must be at most 64 characters long'),
  secret: z.string(),
});

export default defineEventHandler(async (event) => {
  const result = await readValidatedBody(event, body => workerAuthenticateSchema.safeParse(body));
  if (!result.success)
    throw createError({ status: 400, statusMessage: 'Bad Request', message: 'Invalid request body', data: z.treeifyError(result.error) });

  const { worker_id, secret } = result.data;

  const worker = await db.query.worker.findFirst({
    where: eq(schema.worker.id, worker_id),
    columns: {
      id: true,
      secret: true,
      salt: true,
      status: true,
    },
  });

  if (!worker)
    throw createError({ status: 404, statusMessage: 'Not Found', message: 'Worker not found' });

  const hashedSecret = crypto.scryptSync(secret, Buffer.from(worker.salt, 'hex'), 64);
  const match = crypto.timingSafeEqual(hashedSecret, Buffer.from(worker.secret, 'hex'));

  if (!match)
    throw createError({ status: 404, statusMessage: 'Not Found', message: 'Worker not found' });

  if (worker.status !== 'normal')
    throw createError({ status: 403, statusMessage: 'Forbidden', message: 'Worker is disabled' });

  const token = await produceAccessToken(worker.id);
  return { token };
});
