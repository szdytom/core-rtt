<script setup lang="ts">
import z from 'zod';

const open = defineModel<boolean>('open');
defineProps<{
  opponentStrategyGroup?: RouterOutput['leaderboard']['get'][number];
}>();

const { $trpc } = useNuxtApp();
const { data: myStrategyGroups } = await $trpc.strategyGroup.listMine.useQuery();

const schema = z.object({
  myStrategyGroupId: z.string().min(1, 'Base strategy is required'),
});

type Schema = z.output<typeof schema>;

const state = reactive<Schema>({
  myStrategyGroupId: myStrategyGroups.value?.[0]?.id || '',
});
</script>

<template>
  <UModal
    v-model:open="open"
    :title="`Challenge ${opponentStrategyGroup?.name ?? ''}`"
    description="Pick how you want to start the match."
  >
    <template #body>
      <UForm
        v-model="state"
        :schema="schema"
        class="space-y-6"
      >
        <UFormField
          label="Opponent Strategy Group"
          description="The strategy group you will are challenging"
        >
          <div class="ring ring-inset ring-accented px-2.5 py-1.5 flex items-center gap-2 justify-between bg-muted">
            <span class="font-bold">
              {{ opponentStrategyGroup?.name }}
            </span>
            <span>
              by
              <UAvatar
                v-if="opponentStrategyGroup?.user.image"
                :src="opponentStrategyGroup?.user.image"
                size="3xs"
              />
              {{ opponentStrategyGroup?.user.name ?? 'Unknown User' }}
            </span>
          </div>
        </UFormField>

        <UFormField
          label="My Strategy Group"
          description="The strategy group you want to use for this challenge"
          name="myStrategyGroupId"
        >
          <USelect
            v-model="state.myStrategyGroupId"
            label="Select Strategy Group"
            :items="myStrategyGroups?.map(group => ({
              label: group.name,
              value: group.id,
            })) ?? []"
            class="w-full"
          />
        </UFormField>

        <UButton
          type="submit"
          variant="soft"
          icon="ri:flashlight-line"
          class="w-full justify-center"
        >
          Challenge to an unrated game
        </UButton>
      </UForm>
    </template>
  </UModal>
</template>
