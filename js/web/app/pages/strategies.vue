<script setup lang="ts">
import { getQueryKey } from 'trpc-nuxt/client';

const { $trpc } = useNuxtApp();

const { data: userLimit } = await $trpc.strategyGroup.getLimit.useQuery();
const totalActiveGroups = computed(() => strategyGroups.value?.filter(g => g.status === 'normal').length || 0);

const { data: strategies } = await $trpc.strategies.listMine.useQuery();
const baseStrategies = computed(() => strategies.value?.filter(s => s.type === 'base'));
const unitStrategies = computed(() => strategies.value?.filter(s => s.type === 'unit'));

const { data: strategyGroups } = await $trpc.strategyGroup.listMine.useQuery();
const strategyGroupListQueryKey = getQueryKey($trpc.strategyGroup.listMine, undefined);

const deleteLoadingIds = ref<string[]>([]);
const deleteConfirmNames = reactive<Record<string, string>>({});

function isDeleteNameConfirmed(strategyGroupId: string, strategyGroupName: string): boolean {
  return deleteConfirmNames[strategyGroupId] === strategyGroupName;
}

async function onDeleteStrategyGroup(strategyGroupId: string, strategyGroupName: string) {
  if (!isDeleteNameConfirmed(strategyGroupId, strategyGroupName)) {
    const toast = useToast();
    toast.add({ title: 'Error', description: 'Type the strategy group name to confirm deletion.', color: 'error' });
    return;
  }

  deleteLoadingIds.value.push(strategyGroupId);

  try {
    await $trpc.strategyGroup.delete.mutate({ id: strategyGroupId });
    await refreshNuxtData(strategyGroupListQueryKey);
    deleteConfirmNames[strategyGroupId] = '';
  } catch (error) {
    useErrorHandler(error);
  }

  deleteLoadingIds.value = deleteLoadingIds.value.filter(id => id !== strategyGroupId);
}

const toggleLoadingIds = ref<string[]>([]);
async function onToggleActive(strategyGroupId: string) {
  toggleLoadingIds.value.push(strategyGroupId);

  try {
    await $trpc.strategyGroup.toggleActive.mutate({ id: strategyGroupId });
    await refreshNuxtData(strategyGroupListQueryKey);
  } catch (error) {
    useErrorHandler(error);
  }

  toggleLoadingIds.value = toggleLoadingIds.value.filter(id => id !== strategyGroupId);
}
</script>

<template>
  <div class="border-x border-default">
    <div class="flex items-center border-b border-default p-6">
      <h2 class="font-black text-lg underline underline-offset-8">
        Strategy Groups
      </h2>
      <StrategiesGroupForm :strategies="strategies" />
    </div>

    <div>
      <UEmpty
        v-if="!strategyGroups || strategyGroups.length === 0"
        class="text-sm text-muted-foreground"
        variant="naked"
        title="No strategy groups yet"
        description="Create a new strategy group to get started."
        :ui="{ root: 'py-20!' }"
      />
      <div class="grid divide-default divide-y">
        <UPageCard
          v-for="(strategyGroup, index) in strategyGroups"
          :key="strategyGroup.id"
          variant="naked"
        >
          <template #body>
            <div class="px-6 pt-6 pb-2">
              <UBadge
                variant="subtle"
                class="font-bold mb-2"
              >
                Rating: {{ strategyGroup.rating }}
              </UBadge>
              <div class="flex gap-2 items-center font-bold">
                <span class="text-muted">[#{{ index + 1 }}]</span>
                <span>
                  {{ strategyGroup.name }}
                </span>
              </div>
              <div class="flex flex-col mt-1">
                <div>
                  <span class="font-semibold">
                    Base:
                  </span>
                  {{ strategyGroup.strategyBase.name }}
                </div>
                <div>
                  <span class="font-semibold">
                    Unit:
                  </span>
                  {{ strategyGroup.strategyUnit.name }}
                </div>
              </div>
              <div class="absolute top-6 right-6 flex items-center gap-3">
                <UTooltip text="Toggle Active">
                  <USwitch
                    :model-value="strategyGroup.status === 'normal'"
                    :loading="toggleLoadingIds.includes(strategyGroup.id)"
                    loading-icon="ri:loader-2-line"
                    :disabled="strategyGroup.status !== 'normal' && totalActiveGroups >= (userLimit ?? 3)"
                    @change="onToggleActive(strategyGroup.id)"
                  />
                </UTooltip>

                <USeparator
                  orientation="vertical"
                  class="h-6"
                />

                <UModal title="Delete Strategy Group">
                  <UTooltip text="Delete Strategy Group">
                    <UButton
                      color="error"
                      variant="ghost"
                      icon="ri:delete-bin-line"
                      size="sm"
                      :disabled="deleteLoadingIds.includes(strategyGroup.id)"
                      @click="deleteConfirmNames[strategyGroup.id] = ''"
                    />
                  </UTooltip>

                  <template #body>
                    <div class="space-y-6">
                      <p>
                        Are you sure you want to delete
                        <span class="font-semibold">{{ strategyGroup.name }}</span>?
                        This action cannot be undone.
                      </p>

                      <div class="space-y-2">
                        <p class="text-sm text-muted">
                          Type <span class="font-semibold">{{ strategyGroup.name }}</span> to confirm.
                        </p>
                        <UInput
                          :model-value="deleteConfirmNames[strategyGroup.id] || ''"
                          placeholder="Strategy group name"
                          class="w-full"
                          @update:model-value="deleteConfirmNames[strategyGroup.id] = String($event)"
                        />
                      </div>
                    </div>
                  </template>

                  <template #footer>
                    <UButton
                      color="error"
                      variant="soft"
                      icon="ri:delete-bin-line"
                      :disabled="!isDeleteNameConfirmed(strategyGroup.id, strategyGroup.name)"
                      :loading="deleteLoadingIds.includes(strategyGroup.id)"
                      class="w-full"
                      @click="onDeleteStrategyGroup(strategyGroup.id, strategyGroup.name)"
                    >
                      Delete
                    </UButton>
                  </template>
                </UModal>
              </div>
            </div>
          </template>
          <StrategiesRatingChart :show-x-axis="false" />
        </UPageCard>
      </div>
    </div>

    <div class="flex items-center border-y border-default p-6">
      <h2 class="font-black text-lg underline underline-offset-8">
        Strategies
      </h2>
      <StrategiesForm />
    </div>

    <div class="grid md:grid-cols-2 divide-y md:divide-x md:divide-y-0 divide-default border-default border-b">
      <div>
        <div class="p-6 flex items-center gap-4 border-b border-default bg-muted">
          <Icon
            name="ri:base-station-line"
            size="18"
            class="inline-block"
          />
          <span class="font-bold">Base</span>
        </div>
        <UEmpty
          v-if="!baseStrategies || baseStrategies.length === 0"
          class="text-sm text-muted-foreground"
          variant="naked"
          title="No base strategies yet"
          description="Create a new strategy to get started."
          size="sm"
        />
        <div class="grid divide-y divide-default">
          <UPageCard
            v-for="strategy in baseStrategies"
            :key="strategy.id"
            :title="strategy.name"
            :description="strategy.elfFile?.filename"
            variant="naked"
            class="p-6"
          />
        </div>
      </div>
      <div>
        <div class="p-6 flex items-center gap-4 border-b border-default bg-muted">
          <Icon
            name="ri:crosshair-2-line"
            size="18"
            class="inline-block"
          />
          <span class="font-bold">Unit</span>
        </div>
        <UEmpty
          v-if="!unitStrategies || unitStrategies.length === 0"
          class="text-sm text-muted-foreground"
          variant="naked"
          title="No unit strategies yet"
          description="Create a new strategy to get started."
          size="sm"
        />
        <div class="grid divide-y divide-default">
          <UPageCard
            v-for="strategy in unitStrategies"
            :key="strategy.id"
            :title="strategy.name"
            :description="strategy.elfFile?.filename"
            variant="naked"
            class="px-8 py-6"
          />
        </div>
      </div>
    </div>
  </div>
</template>
