<script lang="ts" setup>
const route = useRoute();
const id = route.params.id as string;

const { $trpc } = useNuxtApp();

const { data: match } = await $trpc.match.get.useQuery({ matchId: id });
</script>

<template>
  <div>
    {{ match }}
  </div>
</template>
