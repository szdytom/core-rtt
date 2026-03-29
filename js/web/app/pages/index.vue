<script setup lang="ts">
import { authClient } from '~/lib/auth-client';

const session = authClient.useSession();
</script>

<template>
  <UPageHero
    title="Core RTT"
    description="A real-time tile-based game where players program their units' behavior rather than controlling them directly. The game is designed to be simple and accessible, with a focus on programming and strategy."
    orientation="horizontal"
  >
    <template #links>
      <LayoutLoginButton v-if="!session.data" />
      <UButton
        to="/docs"
        color="neutral"
        variant="subtle"
        trailing-icon="ri:arrow-right-line"
        size="xl"
      >
        Read docs
      </UButton>
    </template>

    <HomeTilemapExample />
  </UPageHero>
</template>
