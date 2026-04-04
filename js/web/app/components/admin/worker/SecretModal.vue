<script setup lang="ts">
const open = defineModel<boolean>('open', { default: false });

defineProps<{
  workerId: string;
  secret: string;
}>();
</script>

<template>
  <UModal
    v-model:open="open"
    title="Worker Created"
    description="Save this secret now. It will not be shown again."
  >
    <template #body>
      <div class="space-y-3">
        <UFormField label="Worker ID">
          <UInput
            :model-value="workerId"
            readonly
            class="w-full"
          />
        </UFormField>

        <UFormField label="Worker Secret">
          <UInput
            :model-value="secret"
            readonly
            class="w-full"
          />
        </UFormField>

        <div class="flex justify-end">
          <UButton @click="open = false">
            Close
          </UButton>
        </div>
      </div>
    </template>
  </UModal>
</template>
