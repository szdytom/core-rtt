<script setup lang="ts">
import * as z from 'zod';

const emit = defineEmits<{
  created: [{ workerId: string; secret: string }];
}>();

const { $trpc } = useNuxtApp();

const modalOpen = ref(false);

const schema = z.object({
  name: z.string().min(1, 'Name is required').max(100, 'Name must be at most 100 characters'),
  description: z.string().max(1000, 'Description must be at most 1000 characters').optional(),
});

type Schema = z.output<typeof schema>;

const state = reactive<Schema>({
  name: '',
  description: '',
});

async function onSubmit() {
  try {
    const { workerId, secret } = await $trpc.worker.create.mutate({
      name: state.name,
      description: state.description || undefined,
    });

    state.name = '';
    state.description = '';
    modalOpen.value = false;

    emit('created', { workerId, secret });
  } catch (error) {
    useErrorHandler(error);
  }
}
</script>

<template>
  <UModal
    v-model:open="modalOpen"
    title="Create Worker"
  >
    <UButton
      icon="ri:add-line"
      variant="subtle"
    >
      Create Worker
    </UButton>

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

        <UButton type="submit">
          Submit
        </UButton>
      </UForm>
    </template>
  </UModal>
</template>
