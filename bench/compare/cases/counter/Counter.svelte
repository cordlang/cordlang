<script>
  let { label = "Counter" } = $props();
  let count = $state(0);
  function setCount(v) { count = v; }
</script>

<div class="flex flex-col gap-4 p-6 items-center">
  <h1 class="text-2xl font-bold">{label}</h1>
  <p class="text-lg text-gray-500">Count: {count}</p>
  <div class="flex gap-2 items-center">
    <button class="btn outline" onclick={() => setCount(count - 1)}>-</button>
    <span class="text-4xl font-bold">{count}</span>
    <button class="btn primary" onclick={() => setCount(count + 1)}>+</button>
  </div>
  <button class="btn ghost" onclick={() => setCount(0)}>Reset</button>
</div>
