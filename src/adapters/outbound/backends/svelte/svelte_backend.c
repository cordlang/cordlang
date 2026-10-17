#include "adapters/outbound/backends/svelte/svelte_backend_internal.h"

static char g_sv_stores[32][64];
static int g_sv_n_stores;

void svelte_sv_reset_stores(void) { g_sv_n_stores = 0; }

void svelte_sv_add_store(const char *name) {
  if (!name || !*name || g_sv_n_stores >= 32) return;
  for (int i = 0; i < g_sv_n_stores; i++)
    if (strcmp(g_sv_stores[i], name) == 0) return;
  snprintf(g_sv_stores[g_sv_n_stores], sizeof(g_sv_stores[0]), "%s", name);
  g_sv_n_stores++;
}

int svelte_sv_is_store_expr(const char *expr) {
  if (!expr || !*expr) return 0;
  for (const char *p = expr; *p; p++) {
    if (!(isalnum((unsigned char)*p) || *p == '_' || *p == '$')) return 0;
  }
  for (int i = 0; i < g_sv_n_stores; i++)
    if (strcmp(g_sv_stores[i], expr) == 0) return 1;
  return 0;
}

int svelte_emit_modules_from_ir(IrProgram *ir, SvelteWriteFn write_fn,
                                void *userdata) {
  if (!ir || !ir->root) return -1;
  SvelteProject *proj = calloc(1, sizeof(SvelteProject));
  if (!proj) return -1;
  svelte_project_partition_from_ir(proj, ir->root);
  if (proj->truncated) {
    free(proj);
    return -1;
  }
  svelte_harvest_contexts_ir(proj, ir->root);

  if (proj->n_contexts > 0) {
    char *ctxm = svelte_gen_contexts_module_ir(proj);
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
    char *mod = svelte_gen_svelte_module_ir(proj, &proj->units[i]);
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

  char *app = svelte_gen_app_svelte_ir(proj);
  if (!app) {
    free(proj);
    return -1;
  }
  int rc = write_fn("App.svelte", app, userdata);
  free(app);
  free(proj);
  return rc;
}

int svelte_emit_modules(Node *root, SvelteWriteFn write_fn, void *userdata) {
  if (!root) return -1;
  IrProgram *ir = ir_from_ast(root, NULL);
  if (!ir) return -1;
  int rc = svelte_emit_modules_from_ir(ir, write_fn, userdata);
  ir_free(ir);
  return rc;
}

char *svelte_svelte_generate_impl_ir(IrProgram *ir) {
  if (!ir || !ir->root) return strdup("<!-- empty -->\n");
  StrBuf sb;
  svelte_sb_init(&sb);
  SvelteProject *proj = calloc(1, sizeof(SvelteProject));
  if (!proj) return strdup("<!-- empty -->\n");
  svelte_project_partition_from_ir(proj, ir->root);
  if (proj->truncated) {
    free(proj);
    return NULL;
  }
  svelte_harvest_contexts_ir(proj, ir->root);

  if (proj->n_contexts > 0) {
    char *ctxm = svelte_gen_contexts_module_ir(proj);
    svelte_sb_append(&sb, "<!-- ===== src/contexts.js ===== -->\n");
    svelte_sb_append(&sb, ctxm ? ctxm : "");
    svelte_sb_append(&sb, "\n");
    free(ctxm);
  }

  for (int i = 0; i < proj->n_units; i++) {
    char *mod = svelte_gen_svelte_module_ir(proj, &proj->units[i]);
    svelte_sb_appendf(&sb, "<!-- ===== src/%s ===== -->\n", proj->units[i].rel);
    svelte_sb_append(&sb, mod ? mod : "");
    svelte_sb_append(&sb, "\n");
    free(mod);
  }
  char *app = svelte_gen_app_svelte_ir(proj);
  svelte_sb_append(&sb, "<!-- ===== src/App.svelte ===== -->\n");
  svelte_sb_append(&sb, app ? app : "");
  free(app);
  free(proj);
  return sb.buf;
}

char *svelte_generate_from_ir(IrProgram *ir) {
  if (!ir || !ir->root) return strdup("<!-- empty -->\n");
  return svelte_svelte_generate_impl_ir(ir);
}

char *svelte_generate(Node *root) {
  if (!root) return strdup("<!-- empty -->\n");
  IrProgram *ir = ir_from_ast(root, NULL);
  if (!ir) return strdup("<!-- empty -->\n");
  char *out = svelte_generate_from_ir(ir);
  ir_free(ir);
  return out;
}
