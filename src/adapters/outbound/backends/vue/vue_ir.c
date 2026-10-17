#include "adapters/outbound/backends/vue/vue_ir_internal.h"

static char g_vu_stores[32][64];
static int g_vu_n_stores;

void vue_vu_reset_stores(void) { g_vu_n_stores = 0; }

void vue_vu_add_store(const char *name) {
  if (!name || !*name || g_vu_n_stores >= 32) return;
  for (int i = 0; i < g_vu_n_stores; i++)
    if (strcmp(g_vu_stores[i], name) == 0) return;
  snprintf(g_vu_stores[g_vu_n_stores], sizeof(g_vu_stores[0]), "%s", name);
  g_vu_n_stores++;
}

int vue_vu_is_store_expr(const char *expr) {
  if (!expr || !*expr) return 0;
  for (const char *p = expr; *p; p++) {
    if (!(isalnum((unsigned char)*p) || *p == '_' || *p == '$')) return 0;
  }
  for (int i = 0; i < g_vu_n_stores; i++)
    if (strcmp(g_vu_stores[i], expr) == 0) return 1;
  return 0;
}

int vue_emit_modules_from_ir(IrProgram *ir, VueWriteFn write_fn,
                                void *userdata) {
  if (!ir || !ir->root) return -1;
  VueProject *proj = calloc(1, sizeof(VueProject));
  if (!proj) return -1;
  vue_project_partition_from_ir(proj, ir->root);
  if (proj->truncated) {
    free(proj);
    return -1;
  }
  vue_harvest_contexts_ir(proj, ir->root);

  if (proj->n_contexts > 0) {
    char *ctxm = vue_gen_contexts_module_ir(proj);
    if (ctxm) {
      int rc = write_fn("contexts.js", ctxm, userdata);
      free(ctxm);
      if (rc != 0) {
        free(proj);
        return rc;
      }
    }
  }

  for (int i = 0; i < proj->n_units; i++) {
    char *mod = vue_gen_vue_module_ir(proj, &proj->units[i]);
    if (!mod) {
      free(proj);
      return -1;
    }
    int rc = write_fn(proj->units[i].rel, mod, userdata);
    free(mod);
    if (rc != 0) {
      free(proj);
      return rc;
    }
  }

  {
    char *rj = vue_gen_router_js_ir(proj);
    if (rj) {
      int rc = write_fn("router.js", rj, userdata);
      free(rj);
      if (rc != 0) {
        free(proj);
        return rc;
      }
    }
  }

  char *app = vue_gen_app_vue_ir(proj);
  if (!app) {
    free(proj);
    return -1;
  }
  int rc = write_fn("App.vue", app, userdata);
  free(app);
  free(proj);
  return rc;
}

char *vue_vue_generate_impl_ir(IrProgram *ir) {
  if (!ir || !ir->root) return strdup("<!-- empty -->\n");
  StrBuf sb;
  vue_sb_init(&sb);
  VueProject *proj = calloc(1, sizeof(VueProject));
  if (!proj) return strdup("<!-- empty -->\n");
  vue_project_partition_from_ir(proj, ir->root);
  if (proj->truncated) {
    free(proj);
    return NULL;
  }
  vue_harvest_contexts_ir(proj, ir->root);

  if (proj->n_contexts > 0) {
    char *ctxm = vue_gen_contexts_module_ir(proj);
    vue_sb_append(&sb, "<!-- ===== src/contexts.js ===== -->\n");
    vue_sb_append(&sb, ctxm ? ctxm : "");
    vue_sb_append(&sb, "\n");
    free(ctxm);
  }

  for (int i = 0; i < proj->n_units; i++) {
    char *mod = vue_gen_vue_module_ir(proj, &proj->units[i]);
    vue_sb_appendf(&sb, "<!-- ===== src/%s ===== -->\n", proj->units[i].rel);
    vue_sb_append(&sb, mod ? mod : "");
    vue_sb_append(&sb, "\n");
    free(mod);
  }
  {
    char *rj = vue_gen_router_js_ir(proj);
    vue_sb_append(&sb, "<!-- ===== src/router.js ===== -->\n");
    vue_sb_append(&sb, rj ? rj : "");
    vue_sb_append(&sb, "\n");
    free(rj);
  }
  char *app = vue_gen_app_vue_ir(proj);
  vue_sb_append(&sb, "<!-- ===== src/App.vue ===== -->\n");
  vue_sb_append(&sb, app ? app : "");
  free(app);
  free(proj);
  return sb.buf;
}

char *vue_generate_from_ir(IrProgram *ir) {
  if (!ir || !ir->root) return strdup("<!-- empty -->\n");
  return vue_vue_generate_impl_ir(ir);
}
