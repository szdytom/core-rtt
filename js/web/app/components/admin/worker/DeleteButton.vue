<script setup lang="ts">
const { workerId, workerName } = defineProps<{
  workerId: string;
  workerName: string;
}>();

const emit = defineEmits<{
  deleted: [];
}>();

const { $trpc } = useNuxtApp();
const modalOpen = ref(false);
const deleteLoading = ref(false);
const confirmDeleteName = ref('');

async function onDeleteWorker() {
  if (confirmDeleteName.value !== workerName) {
    const toast = useToast();
    toast.add({ title: 'Error', description: 'Type the worker name to confirm deletion.', color: 'error' });
    return;
  }

  deleteLoading.value = true;

  try {
    await $trpc.worker.delete.mutate({ id: workerId });
    confirmDeleteName.value = '';
    modalOpen.value = false;
    emit('deleted');
  } catch (error) {
    useErrorHandler(error);
  }

  deleteLoading.value = false;
}
</script>

<template>
  <UModal
    v-model:open="modalOpen"
    title="Delete Worker"
  >
    <UButton
      size="xs"
      variant="ghost"
      color="error"
      icon="ri:delete-bin-line"
      square
      :disabled="deleteLoading"
      @click="confirmDeleteName = ''"
    />

    <template #body>
      <div class="space-y-6">
        <p>
          Are you sure you want to delete the worker <span class="font-semibold">{{ workerName }}</span>?
          This action cannot be undone.
        </p>

        <div class="space-y-2">
          <p class="text-sm text-muted">
            Type <span class="font-semibold">{{ workerName }}</span> to confirm.
          </p>
          <UInput
            v-model="confirmDeleteName"
            placeholder="Worker name"
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
        :disabled="confirmDeleteName !== workerName || deleteLoading"
        :loading="deleteLoading"
        class="w-full"
        @click="onDeleteWorker"
      >
        Delete
      </UButton>
    </template>
  </UModal>
</template>
