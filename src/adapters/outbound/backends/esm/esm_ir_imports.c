#include "adapters/outbound/backends/esm/esm_ir_internal.h"

int esm_ir_url_for_resolved(const char *resolved, const char *project_root,
                            char *out, size_t n) {
  char *nr = fs_norm_path(resolved);
  char *root_abs = project_root ? fs_norm_path(project_root) : NULL;
  int ok = 0;
  if (nr && root_abs) {
    size_t rl = strlen(root_abs);
    while (rl > 0 && root_abs[rl - 1] == '/') rl--;
    if (strncmp(nr, root_abs, rl) == 0 && (nr[rl] == '/' || nr[rl] == '\0')) {
      const char *rest = nr + rl;
      while (*rest == '/') rest++;
      snprintf(out, n, "/%s", rest);
      ok = 1;
    }
  }
  if (!ok && nr) {
    /* Outside the root should have been rejected by the resolver; be explicit. */
    snprintf(out, n, "/%s", nr);
    ok = 1;
  }
  free(nr);
  free(root_abs);
  return ok;
}

EsmImport *esm_ir_import_find_by_name(EsmCtx *c, const char *name) {
  if (!name) return NULL;
  for (int i = 0; i < c->n_imports; i++)
    if (strcmp(c->imports[i].export_name, name) == 0) return &c->imports[i];
  return NULL;
}

const char *esm_ir_import_unresolved(EsmCtx *c, const char *mod_path,
                                     const char *alias) {
  char *ename = compiler_module_export_name(mod_path, alias);
  if (!ename) return NULL;
  for (int i = 0; i < c->n_unresolved; i++) {
    if (strcmp(c->unresolved[i].export_name, ename) == 0) {
      free(ename);
      return c->unresolved[i].export_name;
    }
  }
  if (c->n_unresolved >= 32) {
    free(ename);
    return NULL;
  }
  int i = c->n_unresolved++;
  snprintf(c->unresolved[i].export_name, sizeof(c->unresolved[i].export_name),
           "%s", ename);
  snprintf(c->unresolved[i].mod_path, sizeof(c->unresolved[i].mod_path), "%s",
           mod_path ? mod_path : "?");
  free(ename);
  return c->unresolved[i].export_name;
}

const char *esm_ir_import_add(EsmCtx *c, const char *mod_path, const char *alias) {
  if (!mod_path || !*mod_path) return NULL;

  char *resolved = compiler_resolve_module(c->mod->abs_path, mod_path,
                                          c->mod->project_root);
  if (!resolved) return esm_ir_import_unresolved(c, mod_path, alias);

  char url[512];
  esm_ir_url_for_resolved(resolved, c->mod->project_root, url, sizeof(url));
  free(resolved);

  char *ename = compiler_module_export_name(mod_path, alias);
  if (!ename) return NULL;

  for (int i = 0; i < c->n_imports; i++) {
    if (strcmp(c->imports[i].url, url) == 0 &&
        strcmp(c->imports[i].export_name, ename) == 0) {
      free(ename);
      return c->imports[i].export_name;
    }
  }
  if (c->n_imports >= ESM_MAX_IMPORTS) {
    c->truncated = 1;
    free(ename);
    return NULL;
  }
  EsmImport *im = &c->imports[c->n_imports++];
  snprintf(im->export_name, sizeof(im->export_name), "%s", ename);
  snprintf(im->url, sizeof(im->url), "%s", url);
  snprintf(im->mod_path, sizeof(im->mod_path), "%s", mod_path);
  esm_ir_path_basename_noext(mod_path, im->basename, sizeof(im->basename));
  free(ename);
  return im->export_name;
}

const char *esm_ir_foreign_mod_hint(const IrNode *n) {
  if (!n) return NULL;
  if (n->value && n->value[0]) return n->value;
  for (size_t i = 0; i < n->n_kids; i++) {
    IrNode *a = n->kids[i];
    if (a && a->kind == IR_ATTR && a->value && a->value[0]) return a->value;
  }
  return NULL;
}

void esm_ir_layout_export_name_of(const char *orig, char *out, size_t n) {
  if (!orig || !*orig || strcmp(orig, "default") == 0) {
    snprintf(out, n, "DefaultLayout");
    return;
  }
  if (esm_ir_is_pascal(orig)) {
    snprintf(out, n, "%s", orig);
    return;
  }
  snprintf(out, n, "%c%s", (char)toupper((unsigned char)orig[0]), orig + 1);
}

void esm_ir_foreign_add(EsmCtx *c, const char *name, const char *mod) {
  if (!name || !*name || !esm_ir_is_safe_js_ident(name)) return;
  if (esm_ir_import_find_by_name(c, name)) return;
  for (int i = 0; i < c->n_unresolved; i++)
    if (strcmp(c->unresolved[i].export_name, name) == 0) return;
  for (int i = 0; i < c->n_foreigns; i++)
    if (strcmp(c->foreigns[i].name, name) == 0) return;
  if (c->n_foreigns >= 32) return;
  int i = c->n_foreigns++;
  snprintf(c->foreigns[i].name, sizeof(c->foreigns[i].name), "%s", name);
  snprintf(c->foreigns[i].mod_path, sizeof(c->foreigns[i].mod_path), "%s",
           (mod && mod[0]) ? mod : "");
}

int esm_ir_same_file_export_owns(IrNode *root, const char *name) {
  if (!root || !name || !*name) return 0;
  for (size_t j = 0; j < root->n_kids; j++) {
    IrNode *s = root->kids[j];
    if (!s) continue;
    if (s->kind == IR_COMPONENT && s->name && strcmp(s->name, name) == 0)
      return 1;
    if (s->kind == IR_LAYOUT) {
      char en[96];
      esm_ir_layout_export_name_of(s->name ? s->name : "default", en, sizeof(en));
      if (strcmp(en, name) == 0) return 1;
    }
  }
  return 0;
}

void esm_ir_collect_foreigns(EsmCtx *c, IrNode *root) {
  if (!c || !root) return;
  for (size_t i = 0; i < root->n_kids; i++) {
    IrNode *d = root->kids[i];
    if (!d || d->kind != IR_FOREIGN || !d->name) continue;
    if (esm_ir_same_file_export_owns(root, d->name)) continue;
    esm_ir_foreign_add(c, d->name, esm_ir_foreign_mod_hint(d));
  }
}
