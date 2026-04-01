<script setup lang="ts">
import { getQueryKey } from 'trpc-nuxt/client';

const { strategy } = defineProps<{
  strategy: RouterOutput['strategies']['listMine'][number];
  isUsedInGroups: boolean;
}>();

const deleteLoading = ref(false);
const { $trpc } = useNuxtApp();

async function onDeleteStrategy() {
  deleteLoading.value = true;

  try {
    await $trpc.strategies.delete.mutate({ id: strategy.id });
    const strategyListQueryKey = getQueryKey($trpc.strategies.listMine, undefined);
    await refreshNuxtData(strategyListQueryKey);
  } catch (error) {
    useErrorHandler(error);
  }

  deleteLoading.value = false;
}
</script>

<template>
  <UPageCard
    :title="strategy.name"
    :description="strategy.elfFile?.filename ?? 'No ELF file'"
    variant="naked"
    class="p-6 group"
  >
    <UModal title="Delete Strategy">
      <UTooltip text="Delete Strategy">
        <UButton
          v-if="!isUsedInGroups"
          size="sm"
          variant="ghost"
          icon="ri:delete-bin-line"
          color="error"
          class="group-hover:opacity-100 opacity-0 transition-opacity absolute top-0 right-0"
        />
      </UTooltip>
      <template #body>
        <div class="space-y-6">
          <p>
            Are you sure you want to delete the strategy <span class="font-semibold">{{ strategy.name }}</span>?
            This action cannot be undone.
          </p>
        </div>
      </template>

      <template #footer>
        <UButton
          color="error"
          variant="soft"
          icon="ri:delete-bin-line"
          :loading="deleteLoading"
          class="w-full"
          @click="onDeleteStrategy"
        >
          Delete
        </UButton>
      </template>
    </UModal>
  </UPageCard>
</template>
