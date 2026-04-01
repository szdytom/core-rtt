<script setup lang="ts">
import { getQueryKey } from 'trpc-nuxt/client';
import * as z from 'zod';

const schema = z.object({
  name: z.string().min(1, 'Name is required'),
  type: z.enum(['base', 'unit']),
});
type Schema = z.output<typeof schema>;

const modalOpen = ref(false);

const state = reactive<Partial<Schema>>({
  name: '',
  type: 'base',
});

const elfFile = ref<File | null>(null);

const { $trpc } = useNuxtApp();
const strategyListQueryKey = getQueryKey($trpc.strategies.listMine, undefined);

async function onSubmit() {
  if (!elfFile.value) {
    const toast = useToast();
    toast.add({ title: 'Error', description: 'Please upload an ELF file.', color: 'error' });
    return;
  }

  try {
    const { uploadUrl } = await $trpc.strategies.create.mutate({
      // Zod will not allow undefined values, so we can safely assert that these are not undefined, and provide default values just in case.
      name: state.name ?? 'Unknown Strategy',
      type: state.type ?? 'base',
      fileName: elfFile.value.name,
    });

    await $fetch(uploadUrl, {
      method: 'PUT',
      headers: {
        'Content-Type': elfFile.value.type,
      },
      body: elfFile.value,
    });

    state.name = '';
    state.type = 'base';
    elfFile.value = null;

    await refreshNuxtData(strategyListQueryKey);

    modalOpen.value = false;
  } catch (error) {
    useErrorHandler(error);
  }
}
</script>

<template>
  <UModal
    v-model:open="modalOpen"
    title="Create Strategy"
  >
    <UButton
      icon="ri:add-line"
      size="sm"
      class="ml-auto"
    >
      Create Strategy
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
          label="Type"
          name="type"
        >
          <USelect
            v-model="state.type"
            :items="[
              { label: 'Base', value: 'base' },
              { label: 'Unit', value: 'unit' },
            ]"
          />
        </UFormField>

        <UFormField
          label=".elf File"
          name="elf"
        >
          <UFileUpload
            v-model="elfFile"
            position="inside"
            layout="list"
            label="Drop your ELF file here"
            description="ELF files"
          />
        </UFormField>

        <UButton type="submit">
          Submit
        </UButton>
      </UForm>
    </template>
  </UModal>
</template>
