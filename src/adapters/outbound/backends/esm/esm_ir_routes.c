#include "adapters/outbound/backends/esm/esm_ir_internal.h"

void esm_ir_gen_routes(Sb *sb, IrNode *root, EsmCtx *c) {
  /* Register `use` imports first so layouts are known. */
  char first_layout[96];
  char default_layout[96];
  first_layout[0] = '\0';
  default_layout[0] = '\0';

  for (size_t i = 0; i < root->n_kids; i++) {
    IrNode *u = root->kids[i];
    if (!u || u->kind != IR_MODULE_USE) continue;
    const char *path = u->value;
    const char *alias = u->name;
    const char *ename = esm_ir_import_add(c, path, alias);
    if (!ename) continue;
    char base[96];
    esm_ir_path_basename_noext(path ? path : "", base, sizeof(base));
    int looks_layout = (path && (strstr(path, "layout") || strstr(path, "Layout")));
    if (looks_layout) {
      if (!first_layout[0]) snprintf(first_layout, sizeof(first_layout), "%s", ename);
      if (strcmp(base, "default") == 0)
        snprintf(default_layout, sizeof(default_layout), "%s", ename);
    }
  }

  int n_routes = 0;
  for (size_t i = 0; i < root->n_kids; i++)
    if (root->kids[i] && root->kids[i]->kind == IR_ROUTE) n_routes++;

  if (!n_routes) {
    esm_ir_sb_add(sb, "export const routes = [];\n\n");
    return;
  }

  /*
   * IR_ROUTE carries the URL in `name` and the target module/component in
   * `value` (see ir.c). Resolve targets into imports before emitting the array.
   */
  for (size_t i = 0; i < root->n_kids; i++) {
    IrNode *r = root->kids[i];
    if (!r || r->kind != IR_ROUTE || !r->value) continue;
    if (compiler_is_module_ref(r->value)) esm_ir_import_add(c, r->value, NULL);
  }

  esm_ir_sb_add(sb, "export const routes = [\n");
  for (size_t i = 0; i < root->n_kids; i++) {
    IrNode *r = root->kids[i];
    if (!r || r->kind != IR_ROUTE) continue;

    const char *path = r->name ? r->name : "/";
    char comp[128];
    comp[0] = '\0';
    if (r->value) {
      if (compiler_is_module_ref(r->value)) {
        char *ename = compiler_module_export_name(r->value, NULL);
        if (ename) {
          snprintf(comp, sizeof(comp), "%s", ename);
          free(ename);
        }
      } else {
        snprintf(comp, sizeof(comp), "%s", r->value);
      }
    }
    if (!comp[0]) continue;

    /* layout selection */
    const char *layout_attr = NULL;
    for (size_t j = 0; j < r->n_kids; j++) {
      IrNode *a = r->kids[j];
      if (a && a->kind == IR_ATTR && a->name && strcmp(a->name, "layout") == 0 &&
          a->value && a->value[0])
        layout_attr = a->value;
    }
    char layout[96];
    layout[0] = '\0';
    if (layout_attr) {
      char want[96];
      esm_ir_layout_export_name_of(layout_attr, want, sizeof(want));
      if (esm_ir_import_find_by_name(c, want))
        snprintf(layout, sizeof(layout), "%s", want);
      else {
        for (int k = 0; k < c->n_imports; k++) {
          if (strcmp(c->imports[k].basename, layout_attr) == 0) {
            snprintf(layout, sizeof(layout), "%s", c->imports[k].export_name);
            break;
          }
        }
      }
    } else if (default_layout[0]) {
      snprintf(layout, sizeof(layout), "%s", default_layout);
    } else if (first_layout[0]) {
      snprintf(layout, sizeof(layout), "%s", first_layout);
    }

    esm_ir_sb_add(sb, "  { path: ");
    esm_ir_emit_sq_string(sb, path);
    esm_ir_sb_addf(sb, ", component: %s", comp);
    if (layout[0]) esm_ir_sb_addf(sb, ", layout: %s", layout);
    esm_ir_sb_add(sb, " },\n");
  }
  esm_ir_sb_add(sb, "];\n\n");
}

void esm_ir_gen_imports(Sb *sb, EsmCtx *c) {
  /* `self_mount` (dev entry only) pulls `mount` for the Vite-style boot trailer. */
  if (c->self_mount)
    esm_ir_sb_add(sb,
           "import { h, frag, txt, keyed, component, mount, ErrorBoundary, "
           "Portal, Suspense } from '/@cord/runtime.js';\n");
  else
    esm_ir_sb_add(sb,
           "import { h, frag, txt, keyed, component, ErrorBoundary, Portal, "
           "Suspense } from '/@cord/runtime.js';\n");
  for (int i = 0; i < c->n_imports; i++) {
    const char *q = (c->mod && c->mod->import_query) ? c->mod->import_query : "";
    esm_ir_sb_addf(sb, "import %s from '%s%s';\n", c->imports[i].export_name,
            c->imports[i].url, q);
  }
  esm_ir_sb_add(sb, "\n");
  for (int i = 0; i < c->n_unresolved; i++) {
    char *esc = js_escape_sq_dup(c->unresolved[i].mod_path);
    esm_ir_sb_addf(sb,
            "/* cordlang: modulo no encontrado: %s */\n"
            "const %s = component('%s', function () {\n"
            "  return h('div', { class: 'cord-runtime-error' },\n"
            "           'cordlang: no existe el modulo \\'%s\\'');\n"
            "});\n",
            esc ? esc : "?", c->unresolved[i].export_name,
            c->unresolved[i].export_name, esc ? esc : "?");
    free(esc);
  }
  if (c->n_unresolved) esm_ir_sb_add(sb, "\n");
  for (int i = 0; i < c->n_foreigns; i++) {
    /* A real import for the same name wins (no double-define). */
    if (esm_ir_import_find_by_name(c, c->foreigns[i].name)) continue;
    char *esc_name = js_escape_sq_dup(c->foreigns[i].name);
    const char *mod = c->foreigns[i].mod_path;
    if (mod[0]) {
      char *esc_mod = js_escape_sq_dup(mod);
      esm_ir_sb_addf(sb,
              "/* cordlang: foreign no disponible en preview ESM: %s (%s) */\n"
              "const %s = component('%s', function () {\n"
              "  return h('div', { class: 'cord-runtime-error' },\n"
              "           'cordlang: foreign \\'%s\\' no disponible en preview "
              "ESM');\n"
              "});\n",
              esc_name ? esc_name : "?", esc_mod ? esc_mod : "?",
              c->foreigns[i].name, c->foreigns[i].name,
              esc_name ? esc_name : "?");
      free(esc_mod);
    } else {
      esm_ir_sb_addf(sb,
              "/* cordlang: foreign no disponible en preview ESM: %s */\n"
              "const %s = component('%s', function () {\n"
              "  return h('div', { class: 'cord-runtime-error' },\n"
              "           'cordlang: foreign \\'%s\\' no disponible en preview "
              "ESM');\n"
              "});\n",
              esc_name ? esc_name : "?", c->foreigns[i].name,
              c->foreigns[i].name, esc_name ? esc_name : "?");
    }
    free(esc_name);
  }
  if (c->n_foreigns) esm_ir_sb_add(sb, "\n");
}
