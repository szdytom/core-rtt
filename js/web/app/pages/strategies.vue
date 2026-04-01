<script setup lang="ts">
const { $trpc } = useNuxtApp();

const { data: strategies } = await $trpc.strategies.listMine.useQuery();
const baseStrategies = computed(() => strategies.value?.filter(s => s.type === 'base'));
const unitStrategies = computed(() => strategies.value?.filter(s => s.type === 'unit'));

const { data: strategyGroups } = await $trpc.strategyGroup.listMine.useQuery();
</script>

<template>
  <div class="border-x border-default">
    <div class="flex items-center border-b border-default p-6">
      <h2 class="font-black text-lg underline underline-offset-8">
        Strategy Groups
      </h2>
      <StrategiesGroupCreate :strategies="strategies" />
    </div>

    <StrategiesGroupList
      :strategy-groups="strategyGroups"
    />

    <div class="flex items-center border-y border-default p-6">
      <h2 class="font-black text-lg underline underline-offset-8">
        Strategies
      </h2>
      <StrategiesCreate />
    </div>

    <div class="grid md:grid-cols-2 divide-y md:divide-x md:divide-y-0 divide-default border-default border-b">
      <StrategiesList
        type="base"
        :strategies="baseStrategies"
        :strategy-groups="strategyGroups"
      />
      <StrategiesList
        type="unit"
        :strategies="unitStrategies"
        :strategy-groups="strategyGroups"
      />
    </div>
  </div>
</template>
