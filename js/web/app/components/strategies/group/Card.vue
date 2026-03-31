<script setup lang="ts">
import { getQueryKey } from 'trpc-nuxt/client';

const { strategyGroup } = defineProps<{
  strategyGroup: RouterOutput['strategyGroup']['listMine'][number];
  index: number;
  userLimit?: number;
  totalActiveGroups: number;
}>();

const confirmDeleteName = ref('');
const toggleLoading = ref(false);
const deleteLoading = ref(false);

const { $trpc } = useNuxtApp();
const strategyGroupListQueryKey = getQueryKey($trpc.strategyGroup.listMine, undefined);

async function onToggleActive() {
  toggleLoading.value = true;

  try {
    await $trpc.strategyGroup.toggleActive.mutate({ id: strategyGroup.id });
    await refreshNuxtData(strategyGroupListQueryKey);
  } catch (error) {
    useErrorHandler(error);
  }

  toggleLoading.value = false;
}

async function onDeleteStrategyGroup() {
  if (confirmDeleteName.value !== strategyGroup.name) {
    const toast = useToast();
    toast.add({ title: 'Error', description: 'Type the strategy group name to confirm deletion.', color: 'error' });
    return;
  }

  deleteLoading.value = true;

  try {
    await $trpc.strategyGroup.delete.mutate({ id: strategyGroup.id });
    await refreshNuxtData(strategyGroupListQueryKey);
    confirmDeleteName.value = '';
  } catch (error) {
    useErrorHandler(error);
  }

  deleteLoading.value = false;
}
</script>

<template>
  <UPageCard
    variant="naked"
    class="group"
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
          <UModal title="Delete Strategy Group">
            <UTooltip text="Delete Strategy Group">
              <UButton
                color="error"
                variant="ghost"
                icon="ri:delete-bin-line"
                size="sm"
                :disabled="deleteLoading"
                class="group-hover:opacity-100 opacity-0 transition-opacity"
                @click="confirmDeleteName = ''"
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
                    v-model="confirmDeleteName"
                    placeholder="Strategy group name"
                    class="w-full"
                  />
                </div>
              </div>
            </template>

            <template #footer>
              <UButton
                color="error"
                variant="soft"
                icon="ri:delete-bin-line"
                :disabled="confirmDeleteName !== strategyGroup.name || deleteLoading"
                :loading="deleteLoading"
                class="w-full"
                @click="onDeleteStrategyGroup"
              >
                Delete
              </UButton>
            </template>
          </UModal>

          <USeparator
            orientation="vertical"
            class="h-6 group-hover:opacity-100 opacity-0 transition-opacity"
          />

          <UTooltip text="Toggle Active">
            <USwitch
              :model-value="strategyGroup.status === 'normal'"
              :loading="toggleLoading"
              loading-icon="ri:loader-2-line"
              :disabled="strategyGroup.status !== 'normal' && totalActiveGroups >= (userLimit ?? 3)"
              @change="onToggleActive"
            />
          </UTooltip>
        </div>
      </div>
    </template>
    <StrategiesRatingChart :show-x-axis="false" />
  </UPageCard>
</template>
