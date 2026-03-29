import type { inferRouterInputs, inferRouterOutputs } from '@trpc/server';
import type { AppRouter } from '~~/server/trpc/routers';

export type RouterOutput = inferRouterOutputs<AppRouter>;
export type RouterInput = inferRouterInputs<AppRouter>;
