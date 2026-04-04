import { initTRPC, TRPCError } from '@trpc/server';
import type { H3Event } from 'h3';
import { auth } from '~~/server/lib/auth';
import superjson from 'superjson';
import { ZodError } from 'zod';
import { MemoryStore } from '~~/server/lib/store';
import { env } from '~~/server/env';

export const createTRPCContext = async (event: H3Event) => {
  const authSession = await auth.api.getSession({
    headers: event.headers,
  });
  const fingerprint = await getRequestFingerprint(event) ?? '127.0.0.1';
  return { authSession, fingerprint };
};

type Context = Awaited<ReturnType<typeof createTRPCContext>>;

const t = initTRPC.context<Context>().create({
  transformer: superjson,

  errorFormatter(opts) {
    const { shape, error } = opts;
    return {
      ...shape,
      data: {
        ...shape.data,
        zodError:
          error.code === 'BAD_REQUEST' && error.cause instanceof ZodError
            ? error.cause
            : null,
      },
    };
  },
});

const enforceUserIsAuthed = t.middleware(({ ctx, next }) => {
  if (!ctx.authSession)
    throw new TRPCError({ code: 'UNAUTHORIZED', message: 'You are not logged in.' });

  return next({
    ctx: {
      authSession: ctx.authSession,
    },
  });
});

const store = new MemoryStore({ windowMs: env.RATE_LIMITER_WINDOW_MS });

const rateLimiter = t.middleware(async ({ ctx, next }) => {
  const { totalHits, resetTime } = await store.increment(ctx.fingerprint);

  if (totalHits > env.RATE_LIMITER_MAX_REQUESTS) {
    throw new TRPCError({
      code: 'TOO_MANY_REQUESTS',
      message: `You have exceeded the maximum number of requests. Please try again after ${Math.ceil((resetTime.getTime() - Date.now()) / 1000)} seconds.`,
    });
  }

  return next();
});

const enforceUserIsAdmin = t.middleware(({ ctx, next }) => {
  if (ctx.authSession?.user.role !== 'admin')
    throw new TRPCError({ code: 'FORBIDDEN', message: 'You do not have permission to perform this action.' });

  return next();
});

export const createTRPCRouter = t.router;

export const publicProcedure = t.procedure;
export const protectedProcedure = t.procedure.use(enforceUserIsAuthed);

export const rateLimitedPublicProcedure = publicProcedure.use(rateLimiter);
export const rateLimitedProtectedProcedure = protectedProcedure.use(rateLimiter);

export const adminProcedure = protectedProcedure.use(enforceUserIsAdmin);
