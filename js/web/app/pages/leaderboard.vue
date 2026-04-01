<script setup lang="ts">
import type { TableColumn } from '@nuxt/ui';

const { $trpc } = useNuxtApp();

const page = ref(1);
const pageSize = 20;

const { data: total } = await $trpc.leaderboard.getTotal.useQuery();

const leaderboardPage = ref<RouterOutput['leaderboard']['get']>([]);
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
    cell: ({ row }) => {
      return h('div', { class: row.index < 3 && [topThreeColors[row.index], 'font-bold'] }, [
        `[#${row.index + 1 + (page.value - 1) * pageSize}]`,
      ]);
    },
  },
  {
    accessorKey: 'rating',
    header: 'Rating',
    meta: {
      class: {
        td: 'text-highlighted',
      },
    },
    cell: ({ row }) => {
      return h('div', { class: 'font-mono' }, [
        h('span', { class: 'font-bold' }, [
          row.original.rating.toFixed(0),
        ]),
        h('span', { class: 'text-sm text-muted' }, [
          ` ± ${row.original.ratingDeviation.toFixed(0)}`,
        ]),
      ]);
    },
  },
  {
    accessorKey: 'name',
    header: 'Name',
  },
  {
    accessorFn: row => row.user.name,
    header: 'Creator',
    cell: ({ row }) => {
      return h('div', { class: 'flex items-center gap-2' }, [
        h(UAvatar, {
          src: row.original.user.image,
          loading: 'lazy',
          size: 'xs',
        }),
        h('div', undefined, [
          h('p', row.original.user.name),
        ]),
      ]);
    },
  },
  {
    id: 'expand',
    meta: {
      class: {
        th: 'text-right',
        td: 'text-right',
      },
    },
    cell: ({ row }) =>
      h(UButton, {
        'color': 'neutral',
        'variant': 'ghost',
        'icon': 'ri:arrow-down-s-line',
        'square': true,
        'aria-label': 'Expand',
        'ui': {
          leadingIcon: [
            'transition-transform',
            row.getIsExpanded() ? 'duration-200 rotate-180' : '',
          ],
        },
        'onClick': () => row.toggleExpanded(),
      }),
  },
];
</script>

<template>
  <div class="border-x border-default">
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
      <template #expanded="{ row }">
        <StrategiesRatingChart
          v-if="row.original.ratingHistory.length > 0"
          :rating-history="row.original.ratingHistory"
          show-x-axis
          no-animation
        />
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
