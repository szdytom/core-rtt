<script setup lang="ts">
import type { TableColumn } from '@nuxt/ui';
import { authClient } from '~/lib/auth-client';

const { $trpc } = useNuxtApp();
const session = authClient.useSession();

const page = ref(1);
const pageSize = 20;

const { data: total } = await $trpc.leaderboard.getTotal.useQuery();

const leaderboardPage = ref<RouterOutput['leaderboard']['get']>([]);
const isChallengeMenuOpen = ref(false);
const opponentStrategyGroup = ref<RouterOutput['leaderboard']['get'][number] | undefined>(undefined);

function openChallengeMenu(target: RouterOutput['leaderboard']['get'][number]) {
  opponentStrategyGroup.value = target;
  isChallengeMenuOpen.value = true;
}

watch(page, async (newPage) => {
  const { data } = await $trpc.leaderboard.get.useQuery({
    page: newPage,
    pageSize,
  });
  leaderboardPage.value = data.value || [];
}, {
  immediate: true,
});

const UAvatar = resolveComponent('UAvatar');
const UButton = resolveComponent('UButton');
const topThreeColors = ['text-amber-400', 'text-zinc-500', 'text-yellow-700'];

const columns: TableColumn<RouterOutput['leaderboard']['get'][number]>[] = [
  {
    accessorKey: 'rank',
    header: 'Rank',
  },
  {
    accessorKey: 'rating',
    header: 'Rating',
    meta: {
      class: {
        td: 'text-highlighted',
      },
    },
  },
  {
    accessorKey: 'name',
    header: 'Name',
  },
  {
    accessorKey: 'creator',
    header: 'Creator',
  },
  {
    id: 'challenge',
    meta: {
      class: {
        th: 'text-right',
        td: 'text-right',
      },
    },
  },
];
</script>

<template>
  <div class="border-x border-default">
    <LeaderboardChallengeModal
      v-model:open="isChallengeMenuOpen"
      :opponent-strategy-group="opponentStrategyGroup"
    />

    <UTable
      ref="table"
      :data="leaderboardPage"
      :columns="columns"
      :loading="!leaderboardPage"
      class="border-default border-b"
      :ui="{
        base: 'font-medium',
        separator: 'bg-(--ui-border)',
      }"
    >
      <template #rank-cell="{ row }">
        <div :class="row.index < 3 && [topThreeColors[row.index], 'font-bold']">
          [#{{ row.index + 1 + (page - 1) * pageSize }}]
        </div>
      </template>
      <template #rating-cell="{ row }">
        <div class="font-mono">
          {{ row.original.rating.toFixed(0) }}
          <span class="text-sm text-muted">
            ± {{ row.original.ratingDeviation.toFixed(0) }}
          </span>
        </div>
      </template>
      <template #creator-cell="{ row }">
        <div class="flex items-center gap-2">
          <UAvatar
            :src="row.original.user.image"
            loading="lazy"
            size="xs"
          />
          <div>
            {{ row.original.user.name }}
          </div>
        </div>
      </template>
      <template #challenge-cell="{ row }">
        <div
          v-if="session.data && row.original.user.id !== session.data.user.id"
          class="text-right"
        >
          <UButton
            variant="ghost"
            icon="ri:sword-line"
            square
            size="xs"
            @click="openChallengeMenu(row.original)"
          />
        </div>
      </template>
    </UTable>

    <div class="p-4 flex justify-center border-default border-b">
      <UPagination
        v-model:page="page"
        active-variant="subtle"
        :items-per-page="pageSize"
        :total="total"
      />
    </div>
  </div>
</template>
