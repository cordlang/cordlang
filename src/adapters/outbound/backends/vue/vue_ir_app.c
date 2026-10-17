#include "adapters/outbound/backends/vue/vue_ir_internal.h"

char *vue_gen_router_js_ir(VueProject *proj) {
  StrBuf out;
  vue_sb_init(&out);
  if (!proj || proj->n_routes <= 0) {
    vue_sb_append(&out, "export const router = null;\n");
    return out.buf;
  }
  vue_sb_append(&out, "import { createRouter, createWebHashHistory } from 'vue-router';\n");
  for (int i = 0; i < proj->n_units; i++) {
    if (proj->units[i].kind == SK_LAYOUT)
      vue_sb_appendf(&out, "import %s from './layouts/%s.vue';\n",
                 proj->units[i].name, proj->units[i].name);
  }
  int imported[VU_MAX_UNITS];
  memset(imported, 0, sizeof(imported));
  for (int r = 0; r < proj->n_routes; r++) {
    const char *comp = proj->routes[r]->value;
    if (!comp || vue_route_is_lazy_ir(proj, proj->routes[r])) continue;
    for (int i = 0; i < proj->n_units; i++) {
      if (imported[i]) continue;
      if (strcmp(proj->units[i].name, comp) == 0) {
        vue_sb_appendf(&out, "import %s from './%s/%s.vue';\n",
                   proj->units[i].name, proj->units[i].dir, proj->units[i].name);
        imported[i] = 1;
        break;
      }
    }
  }
  for (int r = 0; r < proj->n_routes; r++) {
    const char *comp = proj->routes[r]->value;
    if (!comp || !vue_route_is_lazy_ir(proj, proj->routes[r])) continue;
    int already = 0;
    for (int prev = 0; prev < r; prev++) {
      if (proj->routes[prev]->value &&
          strcmp(proj->routes[prev]->value, comp) == 0 &&
          vue_route_is_lazy_ir(proj, proj->routes[prev]))
        already = 1;
    }
    if (already) continue;
    const char *dir = "pages";
    for (int i = 0; i < proj->n_units; i++) {
      if (strcmp(proj->units[i].name, comp) == 0) {
        dir = proj->units[i].dir;
        break;
      }
    }
    vue_sb_appendf(&out, "const %s = () => import('./%s/%s.vue');\n", comp, dir, comp);
  }
  vue_sb_append(&out, "\nconst routes = [\n");
  for (int r = 0; r < proj->n_routes; r++) {
    const char *path = proj->routes[r]->name ? proj->routes[r]->name : "/";
    const char *comp =
        proj->routes[r]->value ? proj->routes[r]->value : "HomePage";
    VueUnit *layout = vue_find_layout_for_route_ir(proj, proj->routes[r]);
    if (layout)
      vue_sb_appendf(&out,
                 "  { path: '%s', component: %s, meta: { layout: %s } },\n",
                 path, comp, layout->name);
    else
      vue_sb_appendf(&out, "  { path: '%s', component: %s },\n", path, comp);
  }
  vue_sb_append(&out, "];\n\n");
  vue_sb_append(&out,
            "export const router = createRouter({\n"
            "  history: createWebHashHistory(),\n"
            "  routes,\n"
            "});\n");
  return out.buf;
}

char *vue_gen_app_vue_ir(VueProject *proj) {
  StrBuf out;
  vue_sb_init(&out);

  if (proj->n_routes > 0) {
    vue_sb_append(&out, "<!-- cordlang: source=src/app.cord -->\n");
    vue_sb_append(&out, "<script setup>\n");
    vue_sb_append(&out, "import { RouterView } from 'vue-router';\n");
    vue_sb_append(&out, "import { computed } from 'vue';\n");
    vue_sb_append(&out, "import { router } from './router.js';\n");
    vue_sb_append(&out,
              "const layout = computed(() => router?.currentRoute.value.meta.layout "
              "|| null);\n"
              "</script>\n\n"
              "<template>\n"
              "  <component :is=\"layout\" v-if=\"layout\">\n"
              "    <RouterView />\n"
              "  </component>\n"
              "  <RouterView v-else />\n"
              "</template>\n");
    return out.buf;
  }

  /* No routes: root page component */
  if (proj->n_units > 0) {
    VueUnit *root = &proj->units[0];
    for (int i = 0; i < proj->n_units; i++) {
      if (proj->units[i].kind == SK_PAGE) {
        root = &proj->units[i];
        break;
      }
    }
    vue_sb_append(&out, "<!-- cordlang: source=src/app.cord -->\n");
    vue_sb_append(&out, "<script setup>\n");
    vue_sb_appendf(&out, "import %s from './%s/%s.vue';\n", root->name, root->dir,
               root->name);
    vue_sb_append(&out, "</script>\n\n");
    vue_sb_append(&out, "<template>\n");
    vue_sb_appendf(&out, "  <%s />\n", root->name);
    vue_sb_append(&out, "</template>\n");
    return out.buf;
  }

  vue_sb_append(&out, "<!-- cordlang: source=src/app.cord -->\n");
  vue_sb_append(&out, "<template>\n");
  vue_sb_append(&out, "  <p>Empty Cordlang app</p>\n");
  vue_sb_append(&out, "</template>\n");
  return out.buf;
}
