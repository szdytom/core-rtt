<script setup lang="ts">
import type { DropdownMenuItem } from '@nuxt/ui';
import { authClient } from '~/lib/auth-client';

const session = authClient.useSession();

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

const colorMode = useColorMode();
const dropdownItems = computed<DropdownMenuItem[][]>(() => ([
  [
    {
      label: 'Settings',
    },
  ],
  [
    {
      label: 'Appearance',
      children: [{
        label: 'Light',
        icon: 'ri:sun-line',
        type: 'checkbox',
        checked: colorMode.value === 'light',
        onSelect(e: Event) {
          e.preventDefault();
          colorMode.preference = 'light';
        },
      }, {
        label: 'Dark',
        icon: 'ri:moon-line',
        type: 'checkbox',
        checked: colorMode.value === 'dark',
        onUpdateChecked(checked: boolean) {
          if (checked) {
            colorMode.preference = 'dark';
          }
        },
        onSelect(e: Event) {
          e.preventDefault();
        },
      }],
    },
  ],
  [
    {
      label: 'Logout',
      color: 'error',
      onSelect: () => {
        try {
          authClient.signOut();
        } catch (error) {
          const toast = useToast();
          toast.add({ title: 'Logout failed', description: (error as Error).message, color: 'error' });
        }
      },
    },
  ],
]));
</script>

<template>
  <div v-if="session.data">
    <UDropdownMenu :items="dropdownItems">
      <UButton
        :avatar="{ src: session.data.user.image ?? undefined }"
        variant="ghost"
      >
        {{ session.data.user.name ?? 'My Account' }}
      </UButton>
    </UDropdownMenu>
  </div>
  <UModal v-else>
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
