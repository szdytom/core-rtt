<script setup lang="ts">
import * as z from 'zod';

const schema = z.object({
  name: z.string().min(1, 'Name is required'),
  type: z.enum(['base', 'unit']),
});
type Schema = z.output<typeof schema>;

const state = reactive<Partial<Schema>>({
  name: '',
  type: 'base',
});

async function onSubmit() {
  // TODO
}
</script>

<template>
  <UModal title="Create Strategy">
    <UButton
      icon="ri:add-line"
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
            position="inside"
            layout="list"
            label="Drop your ELF file here"
            description="ELF files"
            accept="application/x-elf"
          />
        </UFormField>

        <UButton type="submit">
          Submit
        </UButton>
      </UForm>
    </template>
  </UModal>
</template>
