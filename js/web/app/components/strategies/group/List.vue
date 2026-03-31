<script setup lang="ts">
const { strategyGroups } = defineProps<{
  strategyGroups?: RouterOutput['strategyGroup']['listMine'];
}>();

const { $trpc } = useNuxtApp();

const { data: userLimit } = await $trpc.strategyGroup.getLimit.useQuery();
const totalActiveGroups = computed(() => strategyGroups?.filter(g => g.status === 'normal').length || 0);
</script>

<template>
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
      <StrategiesGroupCard
        v-for="(strategyGroup, index) in strategyGroups"
        :key="strategyGroup.id"
        :strategy-group="strategyGroup"
        :index="index"
        :user-limit="userLimit"
        :total-active-groups="totalActiveGroups"
      />
    </div>
  </div>
</template>
