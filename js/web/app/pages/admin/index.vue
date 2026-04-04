<script setup lang="ts">
const buttonItems = [
  {
    label: 'Workers Management',
    icon: 'ri:server-line',
    to: '/admin/worker',
  },
];
</script>

<template>
  <div>
    <UPageCard
      v-for="item in buttonItems"
      :key="item.label"
      :title="item.label"
      :icon="item.icon"
      :to="item.to"
    />
  </div>
</template>
