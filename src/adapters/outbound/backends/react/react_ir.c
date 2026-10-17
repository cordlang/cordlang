#include "adapters/outbound/backends/react/react_ir_internal.h"

void react_ir_attach_root_lazies_ir(ReactProject *proj, IrNode *root) {
  if (!root) return;
  for (size_t i = 0; i < root->n_kids; i++) {
    IrNode *c = root->kids[i];
    if (!react_ir_ir_hook_is(c, "lazy") || !c->value || !c->value2) continue;
    if (react_ir_is_route_target_ir(c->value, proj->routes, proj->n_routes)) continue;
    int attached = 0;
    for (int u = 0; u < proj->n_units; u++) {
      int uses = 0;
      for (int k = 0; k < proj->units[u].n_used; k++) {
        if (strcmp(proj->units[u].used[k], c->value) == 0) uses = 1;
      }
      if (uses && proj->units[u].n_lazy < 16) {
        snprintf(proj->units[u].lazy_names[proj->units[u].n_lazy],
                 sizeof(proj->units[u].lazy_names[0]), "%s", c->value);
        snprintf(proj->units[u].lazy_paths[proj->units[u].n_lazy],
                 sizeof(proj->units[u].lazy_paths[0]), "%s", c->value2);
        proj->units[u].n_lazy++;
        proj->units[u].use_lazy = 1;
        attached = 1;
      }
    }
    if (!attached && proj->n_units > 0 && proj->units[0].n_lazy < 16) {
      int u = 0;
      for (int j = 0; j < proj->n_units; j++) {
        if (proj->units[j].kind != RK_LAYOUT) {
          u = j;
          break;
        }
      }
      snprintf(proj->units[u].lazy_names[proj->units[u].n_lazy],
               sizeof(proj->units[u].lazy_names[0]), "%s", c->value);
      snprintf(proj->units[u].lazy_paths[proj->units[u].n_lazy],
               sizeof(proj->units[u].lazy_paths[0]), "%s", c->value2);
      proj->units[u].n_lazy++;
      proj->units[u].use_lazy = 1;
    }
  }
}

int react_emit_modules_from_ir(IrProgram *ir, ReactWriteFn write_fn,
                               void *userdata) {
  if (!ir || !ir->root) return -1;
  ReactProject *proj = calloc(1, sizeof(ReactProject));
  if (!proj) return -1;
  react_ir_project_partition_from_ir(proj, ir);
  if (proj->truncated) {
    free(proj);
    return -1;
  }
  react_ir_attach_root_lazies_ir(proj, ir->root);

  for (int i = 0; i < proj->n_units; i++) {
    IrNode *def = proj->units[i].ir_def;
    if (!def) continue;
    for (size_t j = 0; j < def->n_kids; j++) {
      IrNode *c = def->kids[j];
      if (react_ir_ir_hook_is(c, "context") && c->value && proj->n_contexts < 32) {
        int dup = 0;
        for (int k = 0; k < proj->n_contexts; k++) {
          if (proj->contexts[k]->value &&
              strcmp(proj->contexts[k]->value, c->value) == 0)
            dup = 1;
        }
        if (!dup) proj->contexts[proj->n_contexts++] = c;
      }
    }
  }

  if (proj->n_contexts > 0) {
    char *ctxm = react_ir_gen_contexts_module_ir(proj);
    if (ctxm) {
      int rc = write_fn("contexts.jsx", ctxm, userdata);
      free(ctxm);
      if (rc != 0) {
        free(proj);
        return rc;
      }
    }
  }

  for (int i = 0; i < proj->n_units; i++) {
    char *mod = react_ir_gen_unit_module_ir(proj, &proj->units[i]);
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

  char *app = react_ir_gen_app_module_ir(proj);
  if (!app) {
    free(proj);
    return -1;
  }
  int rc = write_fn("App.jsx", app, userdata);
  free(app);
  free(proj);
  return rc;
}

char *react_ir_react_generate_impl_ir(IrProgram *ir) {
  if (!ir || !ir->root)
    return strdup("export default function App(){return null}\n");

  StrBuf sb;
  react_ir_sb_init(&sb);

  ReactProject *proj = calloc(1, sizeof(ReactProject));
  if (!proj) return strdup("export default function App(){return null}\n");
  react_ir_project_partition_from_ir(proj, ir);
  if (proj->truncated) {
    free(proj);
    return NULL;
  }
  react_ir_attach_root_lazies_ir(proj, ir->root);

  for (int i = 0; i < proj->n_units; i++) {
    IrNode *def = proj->units[i].ir_def;
    if (!def) continue;
    for (size_t j = 0; j < def->n_kids; j++) {
      IrNode *c = def->kids[j];
      if (react_ir_ir_hook_is(c, "context") && c->value && proj->n_contexts < 32) {
        int dup = 0;
        for (int k = 0; k < proj->n_contexts; k++) {
          if (proj->contexts[k]->value &&
              strcmp(proj->contexts[k]->value, c->value) == 0)
            dup = 1;
        }
        if (!dup) proj->contexts[proj->n_contexts++] = c;
      }
    }
  }

  if (proj->n_contexts > 0) {
    char *ctxm = react_ir_gen_contexts_module_ir(proj);
    react_ir_sb_append(&sb, "// ===== src/contexts.jsx =====\n");
    react_ir_sb_append(&sb, ctxm ? ctxm : "");
    react_ir_sb_append(&sb, "\n");
    free(ctxm);
  }

  for (int i = 0; i < proj->n_units; i++) {
    char *mod = react_ir_gen_unit_module_ir(proj, &proj->units[i]);
    react_ir_sb_appendf(&sb, "// ===== src/%s =====\n", proj->units[i].rel);
    react_ir_sb_append(&sb, mod ? mod : "");
    react_ir_sb_append(&sb, "\n");
    free(mod);
  }
  char *app = react_ir_gen_app_module_ir(proj);
  react_ir_sb_append(&sb, "// ===== src/App.jsx =====\n");
  react_ir_sb_append(&sb, app ? app : "");
  free(app);
  free(proj);
  return sb.buf;
}

char *react_generate_from_ir(IrProgram *ir) {
  /* Pure IrNode walk — never uses ir_project_origin for body codegen. */
  if (!ir || !ir->root)
    return strdup("export default function App(){return null}\n");
  return react_ir_react_generate_impl_ir(ir);
}
