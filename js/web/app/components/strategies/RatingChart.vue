<script setup lang="ts">
import { VisXYContainer, VisLine, VisArea, VisCrosshair, VisTooltip, VisAxis } from '@unovis/vue';

const {
  showXAxis = false,
  noAnimation = false,
  ratingHistory,
} = defineProps<{
  showXAxis?: boolean;
  noAnimation?: boolean;
  ratingHistory: { createdAt: Date; rating: number }[];
}>();

const data = computed(() => ratingHistory.map(r => ({ x: r.createdAt, y: r.rating })));

const dataMin = computed(() => data.value.reduce((min, d) => Math.min(min, d.y), data.value[0]?.y || 0));
const dataMax = computed(() => data.value.reduce((max, d) => Math.max(max, d.y), data.value[0]?.y || 0));

const svgDefs = `
    <linearGradient id="gradient" gradientTransform="rotate(90)">
      <stop offset="0%" stop-color="--var(--ui-primary)" stop-opacity="0.12" />
      <stop offset="${100 - ((dataMin.value - 40) / dataMax.value) * 100}%" stop-color="var(--ui-primary)" stop-opacity="0" />
    </linearGradient>
  `;

type DataPoint = { x: Date; y: number };
const template = (d: DataPoint) => `${d.x.toLocaleDateString()}: ${d.y}`;
</script>

<template>
  <ClientOnly>
    <VisXYContainer
      :svg-defs="svgDefs"
      :data="data"
      :y-domain="[dataMin - 40, dataMax]"
      class="h-30"
    >
      <VisLine
        :x="(d: DataPoint) => d.x"
        :y="(d: DataPoint) => d.y"
        color="var(--ui-primary)"
        :line-width="1"
        :duration="noAnimation ? 0 : 600"
      />

      <VisArea
        :x="(d: DataPoint) => d.x"
        :y="(d: DataPoint) => d.y"
        color="url(#gradient)"
        :duration="noAnimation ? 0 : 600"
      />

      <VisAxis
        v-if="showXAxis"
        type="x"
        :tick-format="(x: Date) => new Date(x).toLocaleDateString()"
      />

      <VisCrosshair
        color="var(--ui-primary)"
        :template="template"
      />

      <VisTooltip />
    </VisXYContainer>

    <template #fallback>
      <div class="h-30" />
    </template>
  </ClientOnly>
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
