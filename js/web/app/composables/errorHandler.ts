import type { AppRouter } from '~~/server/trpc/routers';
import { TRPCClientError } from '@trpc/client';

export function useIsTRPCClientError(cause: unknown): cause is TRPCClientError<AppRouter> {
  return cause instanceof TRPCClientError;
}

export async function useErrorHandler(err: unknown): Promise<void> {
  const toast = useToast();

  if (useIsTRPCClientError(err)) {
    if (err.data?.zodError) {
      for (const issue of err.data.zodError.issues) {
        toast.add({ title: 'Error', description: issue.message, color: 'error', icon: 'ri:error-warning-line' });
      }
    } else {
      toast.add({ title: 'Error', description: err.message, color: 'error', icon: 'ri:error-warning-line' });
    }
  } else if (err instanceof Error) {
    toast.add({ title: 'Error', description: err.message, color: 'error', icon: 'ri:error-warning-line' });
  } else {
    toast.add({ title: 'Error', description: 'An error occurred.', color: 'error', icon: 'ri:error-warning-line' });
  }
}
