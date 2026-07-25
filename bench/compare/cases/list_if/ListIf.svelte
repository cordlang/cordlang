<script>
  let { title = "Items", items = [], selectItem } = $props();
  let showHint = $state(1);
</script>

<div class="flex flex-col gap-4 p-6">
  <h1 class="text-2xl font-bold">{title}</h1>
  {#if showHint}
    <p class="text-gray-500">Hint: click items</p>
  {/if}
  {#each items as item (item.id)}
    <div class="flex gap-2 items-center">
      <span class="font-bold">{item.name}</span>
      <button class="btn outline" onclick={() => selectItem(item.id)}>Select</button>
    </div>
  {/each}
</div>
