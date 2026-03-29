<script setup lang="ts">
import { getQueryKey } from 'trpc-nuxt/client';
import * as z from 'zod';

const { strategies } = defineProps<{
  strategies?: RouterOutput['strategies']['list'];
}>();

const modalOpen = ref(false);

const schema = z.object({
  name: z.string().min(1, 'Name is required'),
  strategyBaseId: z.string().min(1, 'Base strategy is required'),
  strategyUnitId: z.string().min(1, 'Unit strategy is required'),
});
type Schema = z.output<typeof schema>;

const state = reactive<Schema>({
  name: '',
  strategyBaseId: strategies?.find(s => s.type === 'base')?.id || '',
  strategyUnitId: strategies?.find(s => s.type === 'unit')?.id || '',
});

const { $trpc } = useNuxtApp();
const strategyGroupListQueryKey = getQueryKey($trpc.strategyGroup.list, undefined);

async function onSubmit() {
  try {
    await $trpc.strategyGroup.create.mutate({
      name: state.name,
      strategyBaseId: state.strategyBaseId,
      strategyUnitId: state.strategyUnitId,
    });

    state.name = '';
    state.strategyBaseId = strategies?.find(s => s.type === 'base')?.id || '';
    state.strategyUnitId = strategies?.find(s => s.type === 'unit')?.id || '';

    await refreshNuxtData(strategyGroupListQueryKey);

    modalOpen.value = false;
  } catch (error) {
    useErrorHandler(error);
  }
}
</script>

<template>
  <UModal
    v-model:open="modalOpen"
    title="Create Strategy Group"
  >
    <UButton
      icon="ri:add-line"
      class="ml-auto"
    >
      Create Strategy Group
    </UButton>

    <template #body>
      <UForm
        :schema="schema"
        :state="state"
        class="space-y-4"
        @submit="onSubmit"
      >
        <UFormField
          label="Name"
          name="name"
        >
          <UInput
            v-model="state.name"
            class="w-full"
          />
        </UFormField>

        <UFormField
          label="Base Strategy"
          name="baseStrategyId"
        >
          <USelect
            v-model="state.strategyBaseId"
            :items="strategies?.filter(s => s.type === 'base').map(s => ({
              label: s.name, value: s.id,
            })) || []"
          />
        </UFormField>

        <UFormField
          label="Unit Strategy"
          name="unitStrategyId"
        >
          <USelect
            v-model="state.strategyUnitId"
            :items="strategies?.filter(s => s.type === 'unit').map(s => ({
              label: s.name, value: s.id,
            })) || []"
          />
        </UFormField>

        <UButton type="submit">
          Submit
        </UButton>
      </UForm>
    </template>
  </UModal>
</template>
