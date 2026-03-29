<script setup lang="ts">
import { VisXYContainer, VisLine, VisArea, VisCrosshair, VisTooltip } from '@unovis/vue';

type DataRecord = { x: number; y: number };
const data = ref<DataRecord[]>(Array.from({ length: 70 }, (_, i) => ({ x: i, y: Math.random() * 1000 + 1000 })));

const dataMin = computed(() => data.value.reduce((min, d) => Math.min(min, d.y), data.value[0]?.y || 0));
const dataMax = computed(() => data.value.reduce((max, d) => Math.max(max, d.y), data.value[0]?.y || 0));

const svgDefs = `
    <linearGradient id="gradient" gradientTransform="rotate(90)">
      <stop offset="0%" stop-color="--var(--ui-primary)" stop-opacity="0.12" />
      <stop offset="${100 - ((dataMin.value - 40) / dataMax.value) * 100}%" stop-color="var(--ui-primary)" stop-opacity="0" />
    </linearGradient>
  `;

const template = (d: DataRecord) => `${d.x}: ${d.y}`;
</script>

<template>
  <VisXYContainer
    :svg-defs="svgDefs"
    :data="data"
    :y-domain="[dataMin - 40, dataMax]"
    class="h-30"
  >
    <VisLine
      :x="(d: any) => d.x"
      :y="(d: any) => d.y"
      color="var(--ui-primary)"
      :line-width="1"
    />

    <VisArea
      :x="(d: any) => d.x"
      :y="(d: any) => d.y"
      color="url(#gradient)"
    />

    <VisCrosshair
      color="var(--ui-primary)"
      :template="template"
    />

    <VisTooltip />
  </VisXYContainer>
</template>

<style scoped>
.unovis-xy-container {
  --vis-crosshair-line-stroke-color: var(--ui-primary);
  --vis-crosshair-circle-stroke-color: var(--ui-bg);

  --vis-axis-grid-color: var(--ui-border);
  --vis-axis-tick-color: var(--ui-border);
  --vis-axis-tick-label-color: var(--ui-text-dimmed);

  --vis-tooltip-background-color: var(--ui-bg);
  --vis-tooltip-border-color: var(--ui-border);
  --vis-tooltip-text-color: var(--ui-text-highlighted);
}
</style>
