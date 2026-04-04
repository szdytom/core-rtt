<script setup lang="ts">
import type { DropdownMenuItem, NavigationMenuItem } from '@nuxt/ui';
import { authClient } from '~/lib/auth-client';

const session = authClient.useSession();

const topNavigationItems = computed<NavigationMenuItem[][]>(() => ([
  [
    {
      label: 'Leaderboard',
      icon: 'ri:bar-chart-2-line',
      to: '/leaderboard',
    },
    {
      label: 'Documentation',
      icon: 'ri:book-2-line',
      to: '/docs',
    },
    session.value.data
      ? {
          label: 'My Strategies',
          icon: 'ri:lightbulb-line',
          to: '/strategies',
        }
      : {},
  ],
]));

const colorMode = useColorMode();
const dropdownItems = computed<DropdownMenuItem[][]>(() => ([
  [
    session.value.data?.user.role === 'admin'
      ? {
          label: 'Admin Panel',
          to: '/admin',
        }
      : {},
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
          navigateTo('/');
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
  <UApp :toaster="{ position: 'top-center', progress: false }">
    <UHeader>
      <template #left>
        <NuxtLink to="/">
          <span class="text-lg font-bold md:p-6">Core RTT</span>
        </NuxtLink>
      </template>

      <UNavigationMenu :items="topNavigationItems" />

      <template #right>
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

        <LayoutLoginButton v-else />
      </template>

      <template #body>
        <UNavigationMenu
          :items="topNavigationItems"
          orientation="vertical"
        />
      </template>
    </UHeader>

    <UMain>
      <UContainer>
        <slot />
      </UContainer>
    </UMain>

    <!-- -mt-px to prevent overlap with bottom border in the page content -->
    <USeparator class="-mt-px!" />

    <UFooter>
      <template #left>
        <p class="text-sm text-muted">
          Built with Nuxt UI • © {{ new Date().getFullYear() }}
        </p>
      </template>

      <template #right>
        <UButton
          to="https://github.com/szdytom/core-rtt"
          target="_blank"
          icon="ri:github-line"
          aria-label="GitHub"
          color="neutral"
          variant="ghost"
        />
      </template>
    </UFooter>
  </UApp>
</template>
