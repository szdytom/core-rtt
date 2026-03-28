<script setup lang="ts">
import type { NavigationMenuItem } from '@nuxt/ui';

const topNavigationItems = ref<NavigationMenuItem[][]>([
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
  ],
]);
</script>

<template>
  <UApp>
    <UHeader>
      <template #left>
        <NuxtLink to="/">
          <span class="text-lg font-bold">Core RTT</span>
        </NuxtLink>
      </template>

      <UNavigationMenu :items="topNavigationItems" />

      <template #right>
        <LayoutLoginButton />
      </template>
    </UHeader>

    <UMain>
      <slot />
    </UMain>

    <USeparator />

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
