#include "adapters/outbound/backends/svelte/svelte_backend_internal.h"

char *svelte_gen_app_svelte_ir(SvelteProject *proj) {
  StrBuf out;
  svelte_sb_init(&out);
  svelte_sb_append(&out, "<!-- cordlang: source=src/app.cord -->\n");
  svelte_sb_append(&out, "<script>\n");
  svelte_sb_append(&out, "  import { onMount } from 'svelte';\n");

  int n_layouts = 0;
  for (int i = 0; i < proj->n_units; i++) {
    if (proj->units[i].kind == SK_LAYOUT) {
      svelte_sb_appendf(&out, "  import %s from './layouts/%s.svelte';\n",
                 proj->units[i].name, proj->units[i].name);
      n_layouts++;
    }
  }

  int any_lazy = 0;
  for (int r = 0; r < proj->n_routes; r++) {
    if (svelte_route_is_lazy_ir(proj, proj->routes[r])) {
      any_lazy = 1;
      break;
    }
  }

  int imported[SV_MAX_UNITS];
  memset(imported, 0, sizeof(imported));

  for (int r = 0; r < proj->n_routes; r++) {
    const char *comp = proj->routes[r]->value;
    if (!comp || svelte_route_is_lazy_ir(proj, proj->routes[r])) continue;
    for (int i = 0; i < proj->n_units; i++) {
      if (imported[i]) continue;
      if (strcmp(proj->units[i].name, comp) == 0) {
        svelte_sb_appendf(&out, "  import %s from './%s/%s.svelte';\n",
                   proj->units[i].name, proj->units[i].dir, proj->units[i].name);
        imported[i] = 1;
        break;
      }
    }
  }

  for (int r = 0; r < proj->n_routes; r++) {
    const char *comp = proj->routes[r]->value;
    if (!comp || !svelte_route_is_lazy_ir(proj, proj->routes[r])) continue;
    int already = 0;
    for (int prev = 0; prev < r; prev++) {
      if (proj->routes[prev]->value &&
          strcmp(proj->routes[prev]->value, comp) == 0 &&
          svelte_route_is_lazy_ir(proj, proj->routes[prev]))
        already = 1;
    }
    if (already) continue;
    const char *dir = "pages";
    for (int i = 0; i < proj->n_units; i++) {
      if (strcmp(proj->units[i].name, comp) == 0) {
        dir = proj->units[i].dir;
        imported[i] = 1;
        break;
      }
    }
    svelte_sb_appendf(&out,
               "  const %s = () => import('./%s/%s.svelte').then(m => m.default);\n",
               comp, dir, comp);
  }

  if (proj->n_routes > 0) {
    svelte_sb_append(&out, "\n  const routes = [\n");
    for (int r = 0; r < proj->n_routes; r++) {
      const char *path = proj->routes[r]->name ? proj->routes[r]->name : "/";
      const char *comp =
          proj->routes[r]->value ? proj->routes[r]->value : "HomePage";
      SvelteUnit *layout = svelte_find_layout_for_route_ir(proj, proj->routes[r]);
      int lazy = svelte_route_is_lazy_ir(proj, proj->routes[r]);
      if (any_lazy) {
        if (layout)
          svelte_sb_appendf(&out,
                     "    { path: '%s', component: %s, layout: %s, lazy: %s },\n",
                     path, comp, layout->name, lazy ? "true" : "false");
        else
          svelte_sb_appendf(&out,
                     "    { path: '%s', component: %s, layout: null, lazy: %s },\n",
                     path, comp, lazy ? "true" : "false");
      } else if (layout) {
        svelte_sb_appendf(&out, "    { path: '%s', component: %s, layout: %s },\n",
                   path, comp, layout->name);
      } else {
        svelte_sb_appendf(&out, "    { path: '%s', component: %s, layout: null },\n",
                   path, comp);
      }
    }
    svelte_sb_append(&out, "  ];\n\n");
    svelte_sb_append(&out,
              "  function currentPath() {\n"
              "    const raw = window.location.hash.replace(/^#/, '') || '/';\n"
              "    const h = raw.split('?')[0] || '/';\n"
              "    return h.startsWith('/') ? h : '/' + h;\n"
              "  }\n"
              "  function matchRoute(pathname) {\n"
              "    const segs = pathname.replace(/\\/+$/, '').split('/').filter(Boolean);\n"
              "    for (const r of routes) {\n"
              "      const pp = r.path.replace(/\\/+$/, '').split('/').filter(Boolean);\n"
              "      if (pp.length !== segs.length) continue;\n"
              "      const params = {};\n"
              "      let ok = true;\n"
              "      for (let i = 0; i < pp.length; i++) {\n"
              "        if (pp[i].startsWith(':')) params[pp[i].slice(1)] = decodeURIComponent(segs[i]);\n"
              "        else if (pp[i] !== segs[i]) { ok = false; break; }\n"
              "      }\n");
    if (any_lazy)
      svelte_sb_append(&out,
                "      if (ok) return { component: r.component, layout: r.layout, lazy: r.lazy, params };\n");
    else
      svelte_sb_append(&out,
                "      if (ok) return { component: r.component, layout: r.layout, params };\n");
    svelte_sb_append(&out,
              "    }\n"
              "    return null;\n"
              "  }\n"
              "  let path = $state(currentPath());\n"
              "  onMount(() => {\n"
              "    const onHash = () => { path = currentPath(); };\n"
              "    window.addEventListener('hashchange', onHash);\n"
              "    if (!window.location.hash) window.location.hash = '#/';\n"
              "    return () => window.removeEventListener('hashchange', onHash);\n"
              "  });\n"
              "  let matched = $derived(matchRoute(path) ?? matchRoute('/') ?? null);\n"
              "  let Page = $derived(matched?.component ?? null);\n"
              "  let Layout = $derived(matched?.layout ?? null);\n");
    if (any_lazy)
      svelte_sb_append(&out, "  let isLazy = $derived(matched?.lazy ?? false);\n");
    svelte_sb_append(&out, "  let params = $derived(matched?.params ?? {});\n");
  }

  svelte_sb_append(&out, "</script>\n\n");

  if (proj->n_routes > 0) {
    if (n_layouts > 0) {
      svelte_sb_append(&out, "{#if Layout && Page}\n");
      svelte_sb_append(&out, "  <Layout>\n");
      svelte_sb_append(&out, "    {#snippet children()}\n");
      if (any_lazy) {
        svelte_sb_append(&out,
                  "      {#if isLazy}\n"
                  "        {#await Page()}\n"
                  "          <p class=\"text-gray-500 p-4\">Cargando...</p>\n"
                  "        {:then Comp}\n"
                  "          <Comp {...params} />\n"
                  "        {:catch}\n"
                  "          <p class=\"text-red-500 p-4\">Error al cargar</p>\n"
                  "        {/await}\n"
                  "      {:else}\n"
                  "        <Page {...params} />\n"
                  "      {/if}\n");
      } else {
        svelte_sb_append(&out, "      <Page {...params} />\n");
      }
      svelte_sb_append(&out, "    {/snippet}\n");
      svelte_sb_append(&out, "  </Layout>\n");
      svelte_sb_append(&out, "{:else if Page}\n");
      if (any_lazy) {
        svelte_sb_append(&out,
                  "  {#if isLazy}\n"
                  "    {#await Page()}\n"
                  "      <p class=\"text-gray-500 p-4\">Cargando...</p>\n"
                  "    {:then Comp}\n"
                  "      <Comp {...params} />\n"
                  "    {:catch}\n"
                  "      <p class=\"text-red-500 p-4\">Error al cargar</p>\n"
                  "    {/await}\n"
                  "  {:else}\n"
                  "    <Page {...params} />\n"
                  "  {/if}\n");
      } else {
        svelte_sb_append(&out, "  <Page {...params} />\n");
      }
      svelte_sb_append(&out, "{/if}\n");
    } else {
      svelte_sb_append(&out, "{#if Page}\n");
      if (any_lazy) {
        svelte_sb_append(&out,
                  "  {#if isLazy}\n"
                  "    {#await Page()}\n"
                  "      <p class=\"text-gray-500 p-4\">Cargando...</p>\n"
                  "    {:then Comp}\n"
                  "      <Comp {...params} />\n"
                  "    {:catch}\n"
                  "      <p class=\"text-red-500 p-4\">Error al cargar</p>\n"
                  "    {/await}\n"
                  "  {:else}\n"
                  "    <Page {...params} />\n"
                  "  {/if}\n");
      } else {
        svelte_sb_append(&out, "  <Page {...params} />\n");
      }
      svelte_sb_append(&out, "{/if}\n");
    }
  } else if (proj->n_units > 0) {
    SvelteUnit *root = &proj->units[0];
    for (int i = 0; i < proj->n_units; i++) {
      if (proj->units[i].kind == SK_PAGE) {
        root = &proj->units[i];
        break;
      }
    }
    free(out.buf);
    svelte_sb_init(&out);
    svelte_sb_append(&out, "<!-- cordlang: source=src/app.cord -->\n");
    svelte_sb_append(&out, "<script>\n");
    svelte_sb_appendf(&out, "  import %s from './%s/%s.svelte';\n", root->name, root->dir,
               root->name);
    svelte_sb_append(&out, "</script>\n\n");
    svelte_sb_appendf(&out, "<%s />\n", root->name);
  } else {
    svelte_sb_append(&out, "<p>Empty Cordlang app</p>\n");
  }
  return out.buf;
}
