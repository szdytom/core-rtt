<script setup lang="ts">
import { getQueryKey } from 'trpc-nuxt/client';
import * as z from 'zod';

const schema = z.object({
  name: z.string().min(1, 'Name is required'),
  type: z.enum(['base', 'unit']),
  llmDisclosure: z.object({
    model: z.string().max(80).optional(),
  }),
});
type Schema = z.output<typeof schema>;

const modalOpen = ref(false);

const state = reactive<Schema>({
  name: '',
  type: 'base',
  llmDisclosure: {
    model: undefined,
  },
});

const elfFile = ref<File | null>(null);

const { $trpc } = useNuxtApp();
const strategyListQueryKey = getQueryKey($trpc.strategies.listMine, undefined);

async function onSubmit() {
  if (!elfFile.value) {
    const toast = useToast();
    toast.add({ icon: 'ri:error-warning-line', title: 'Error', description: 'Please upload an ELF file.', color: 'error' });
    return;
  }

  // Limit ELF file size to 500 KB
  if (elfFile.value.size > 500 * 1024) {
    const toast = useToast();
    toast.add({ icon: 'ri:error-warning-line', title: 'Error', description: 'ELF file too large.', color: 'error' });
    return;
  }

  // Basic validation to check if the file is an ELF file by checking the magic number
  const bytes = await elfFile.value.bytes();
  console.log(elfFile.value);
  if (elfFile.value.size < 4
    || bytes[0] !== 0x7f
    || bytes[1] !== 'E'.charCodeAt(0)
    || bytes[2] !== 'L'.charCodeAt(0)
    || bytes[3] !== 'F'.charCodeAt(0)
  ) {
    const toast = useToast();
    toast.add({ icon: 'ri:error-warning-line', title: 'Error', description: 'Invalid ELF file.', color: 'error' });
    return;
  }

  try {
    const { uploadUrl } = await $trpc.strategies.create.mutate({
      // Zod will not allow undefined values, so we can safely assert that these are not undefined, and provide default values just in case.
      name: state.name ?? 'Unknown Strategy',
      type: state.type ?? 'base',
      fileName: elfFile.value.name,
      llmDisclosure: {
        model: state.llmDisclosure.model,
      },
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

const { data: modelList } = await useLazyFetch<{ data: { name: string }[] }>('https://openrouter.ai/api/v1/models');

function onCreateCustomName(item: string) {
  modelList.value?.data.push({ name: item });
  state.llmDisclosure.model = item;
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
            :ui="{
              base: 'border-solid',
            }"
          />
        </UFormField>

        <UCollapsible>
          <UButton
            variant="ghost"
            trailing-icon="i-lucide-chevron-down"
            class="group w-full"
            block
            :ui="{
              base: 'bg-muted border border-accented',
              trailingIcon: 'group-data-[state=open]:rotate-180 transition-transform duration-200',
            }"
          >
            LLM Usage Disclosure
          </UButton>
          <template #content>
            <div class="p-3 border-x border-b border-accented bg-muted space-y-3">
              <UFormField
                name="model"
                label="Model Used"
                description="Indicate which LLM model was used in the creation of this strategy. Just for fun, not required at all."
                hint="(optional)"
                :ui="{
                  description: 'text-xs my-2',
                }"
              >
                <UInputMenu
                  v-model="state.llmDisclosure.model"
                  :items="modelList?.data.map(model => model.name) ?? []"
                  create-item
                  virtualize
                  class="w-full"
                  @create="onCreateCustomName"
                />
              </UFormField>
            </div>
          </template>
        </UCollapsible>

        <UButton type="submit">
          Submit
        </UButton>
      </UForm>
    </template>
  </UModal>
</template>
