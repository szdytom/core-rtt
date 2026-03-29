<script setup lang="ts">
import { authClient } from '~/lib/auth-client';

const loginProviders = [{
  label: 'GitHub',
  icon: 'ri:github-fill',
  onClick: () => {
    try {
      authClient.signIn.social({
        provider: 'github',
      });
    } catch (error) {
      const toast = useToast();
      toast.add({ title: 'Login failed', description: (error as Error).message, color: 'error' });
    }
  },
}];
</script>

<template>
  <UModal>
    <UButton>
      Login
    </UButton>

    <template #content>
      <UAuthForm
        title="Login"
        description="Login to core-rtt with your GitHub account."
        :providers="loginProviders"
        class="p-8"
      />
    </template>
  </UModal>
</template>
