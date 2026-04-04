<script setup lang="ts">
import * as z from 'zod';

const { workerId, workerName, workerDescription, workerStatus } = defineProps<{
  workerId: string;
  workerName: string;
  workerDescription?: string | null;
  workerStatus: 'normal' | 'disabled';
}>();

const emit = defineEmits<{
  modified: [];
}>();

const { $trpc } = useNuxtApp();
const modalOpen = ref(false);
const modifyLoading = ref(false);

const schema = z.object({
  name: z.string().min(1, 'Name is required').max(100, 'Name must be at most 100 characters'),
  description: z.string().max(1000, 'Description must be at most 1000 characters').optional(),
  status: z.enum(['normal', 'disabled']),
});

type Schema = z.output<typeof schema>;

const state = reactive<Schema>({
  name: workerName,
  description: workerDescription || '',
  status: workerStatus,
});

watch(modalOpen, (isOpen) => {
  if (!isOpen)
    return;

  state.name = workerName;
  state.description = workerDescription || '';
  state.status = workerStatus;
});

async function onSubmit() {
  modifyLoading.value = true;

  try {
    await $trpc.worker.update.mutate({
      id: workerId,
      name: state.name,
      description: state.description || undefined,
      status: state.status,
    });

    modalOpen.value = false;
    emit('modified');
  } catch (error) {
    useErrorHandler(error);
  }

  modifyLoading.value = false;
}
</script>

<template>
  <UModal
    v-model:open="modalOpen"
    title="Modify Worker"
  >
    <UButton
      size="xs"
      variant="ghost"
      color="neutral"
      icon="ri:edit-line"
      square
      :disabled="modifyLoading"
    />

    <template #body>
      <UForm
        :schema="schema"
        :state="state"
        class="space-y-6"
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
          label="Description"
          name="description"
        >
          <UTextarea
            v-model="state.description"
            class="w-full"
            :rows="4"
          />
        </UFormField>

        <UFormField
          label="Status"
          name="status"
        >
          <USelect
            v-model="state.status"
            :items="[
              { label: 'Normal', value: 'normal' },
              { label: 'Disabled', value: 'disabled' },
            ]"
            class="w-full"
          />
        </UFormField>

        <UButton
          type="submit"
          :loading="modifyLoading"
        >
          Save
        </UButton>
      </UForm>
    </template>
  </UModal>
</template>
