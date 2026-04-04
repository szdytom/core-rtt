<script setup lang="ts">
import { getQueryKey } from 'trpc-nuxt/client';
import type { TableColumn } from '@nuxt/ui';

const { $trpc } = useNuxtApp();

const { data: workerList } = await $trpc.worker.list.useQuery();
const workerListQueryKey = getQueryKey($trpc.worker.list, undefined);

const secretModalOpen = ref(false);
const createdWorkerSecret = ref('');
const createdWorkerId = ref('');

const UAvatar = resolveComponent('UAvatar');
const UBadge = resolveComponent('UBadge');

const columns: TableColumn<RouterOutput['worker']['list']['workers'][number]>[] = [
  {
    accessorKey: 'name',
    header: 'Name',
  },
  {
    accessorKey: 'status',
    header: 'Status',
  },
  {
    accessorKey: 'creator',
    header: 'Creator',
  },
  {
    accessorKey: 'createdAt',
    header: 'Created',
  },
  {
    id: 'actions',
    header: 'Actions',
    meta: {
      class: {
        th: 'text-right',
        td: 'text-right',
      },
    },
  },
];

async function onWorkerCreated({ workerId, secret }: { workerId: string; secret: string }) {
  createdWorkerId.value = workerId;
  createdWorkerSecret.value = secret;
  secretModalOpen.value = true;
  await refreshNuxtData(workerListQueryKey);
}

async function onWorkerDeleted() {
  await refreshNuxtData(workerListQueryKey);
}

async function onWorkerModified() {
  await refreshNuxtData(workerListQueryKey);
}
</script>

<template>
  <div>
    <UPageCard
      title="Worker List"
      description="List of all registered workers."
    >
      <template #header>
        <AdminWorkerCreateModal @created="onWorkerCreated" />
      </template>
      <AdminWorkerSecretModal
        v-model:open="secretModalOpen"
        :secret="createdWorkerSecret"
        :worker-id="createdWorkerId"
      />
    </UPageCard>

    <UTable
      :data="workerList?.workers || []"
      :columns="columns"
      class="ring ring-default"
    >
      <template #name-cell="{ row }">
        <UTooltip :text="row.original.description || 'No description'">
          <span class="underline decoration-dotted decoration-muted underline-offset-4">
            {{ row.original.name }}
          </span>
        </UTooltip>
      </template>
      <template #status-cell="{ row }">
        <UBadge
          :color="row.original.status === 'normal' ? 'success' : 'neutral'"
          variant="subtle"
        >
          {{ row.original.status }}
        </UBadge>
      </template>
      <template #creator-cell="{ row }">
        <div class="flex items-center gap-2">
          <UAvatar
            :src="row.original.creator?.image || undefined"
            loading="lazy"
            size="xs"
          />
          <span>
            {{ row.original.creator?.name || '-' }}
          </span>
        </div>
      </template>
      <template #createdAt-cell="{ row }">
        {{ new Date(row.original.createdAt).toLocaleDateString('en-US') }}
      </template>
      <template #actions-cell="{ row }">
        <div class="flex justify-end gap-1">
          <AdminWorkerModifyButton
            :worker-id="row.original.id"
            :worker-name="row.original.name"
            :worker-description="row.original.description"
            :worker-status="row.original.status"
            @modified="onWorkerModified"
          />
          <AdminWorkerDeleteButton
            :worker-id="row.original.id"
            :worker-name="row.original.name"
            @deleted="onWorkerDeleted"
          />
        </div>
      </template>
    </UTable>
  </div>
</template>
