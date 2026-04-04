import { authClient } from '~/lib/auth-client';

export default defineNuxtRouteMiddleware(async (to) => {
  const { data: session } = await authClient.useSession(useFetch);
  if (!session.value) {
    if (to.path === '/strategies') {
      return navigateTo('/');
    }
  }

  if (session.value?.user.role !== 'admin') {
    if (to.path === '/admin') {
      return navigateTo('/');
    }
  }
});
