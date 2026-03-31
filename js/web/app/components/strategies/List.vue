<script setup lang="ts">
defineProps<{
  type: 'base' | 'unit';
  strategies?: RouterOutput['strategies']['listMine'];
  strategyGroups?: RouterOutput['strategyGroup']['listMine'];
}>();
</script>

<template>
  <div>
    <div class="p-6 flex items-center gap-4 border-b border-default bg-muted">
      <Icon
        :name="type === 'base' ? 'ri:base-station-line' : 'ri:crosshair-2-line'"
        size="18"
        class="inline-block"
      />
      <span class="font-bold">{{ type === 'base' ? 'Base' : 'Unit' }}</span>
    </div>
    <UEmpty
      v-if="!strategies || strategies.length === 0"
      class="text-sm text-muted-foreground"
      variant="naked"
      title="No base strategies yet"
      description="Create a new strategy to get started."
      size="sm"
    />
    <div class="grid divide-y divide-default">
      <StrategiesCard
        v-for="strategy in strategies"
        :key="strategy.id"
        :strategy="strategy"
        :is-used-in-groups="strategyGroups?.some(g => g.strategyBaseId === strategy.id || g.strategyUnitId === strategy.id) ?? true"
      />
    </div>
  </div>
</template>
