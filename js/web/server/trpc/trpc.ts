import { initTRPC, TRPCError } from '@trpc/server';
import type { H3Event } from 'h3';
import { auth } from '~~/server/lib/auth';
import superjson from 'superjson';
import { ZodError } from 'zod';

export const createTRPCContext = async (event: H3Event) => {
  const authSession = await auth.api.getSession({
    headers: event.headers,
  });
  return { authSession };
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

export const enforceUserIsAuthed = t.middleware(({ ctx, next }) => {
  if (!ctx.authSession)
    throw new TRPCError({ code: 'UNAUTHORIZED', message: 'You are not logged in.' });

  return next({
    ctx: {
      authSession: ctx.authSession,
    },
  });
});

export const createTRPCRouter = t.router;
export const publicProcedure = t.procedure;
export const protectedProcedure = t.procedure.use(enforceUserIsAuthed);
