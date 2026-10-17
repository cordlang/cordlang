#include "application/symbols_internal.h"

/* ── LSP rename / references ─────────────────────────────────────────── */

void symbol_loc_list_init(SymbolLocList *l) {
  if (!l) return;
  memset(l, 0, sizeof(*l));
}

void symbol_loc_list_free(SymbolLocList *l) {
  if (!l) return;
  for (size_t i = 0; i < l->len; i++) free(l->items[i].path);
  free(l->items);
  free(l->decl_path);
  memset(l, 0, sizeof(*l));
}

static const char *last_seg(const char *path, size_t *len) {
  if (!path || !*path) return NULL;
  const char *slash = strrchr(path, '/');
  const char *bslash = strrchr(path, '\\');
  if (bslash && (!slash || bslash > slash)) slash = bslash;
  const char *base = slash ? slash + 1 : path;
  size_t n = strlen(base);
  if (n > 5 && strcmp(base + n - 5, ".cord") == 0) n -= 5;
  if (len) *len = n;
  return base;
}

static int last_seg_eq(const char *path, const char *name) {
  size_t n = 0;
  const char *s = last_seg(path, &n);
  if (!s || !name) return 0;
  return n == strlen(name) && strncmp(s, name, n) == 0;
}

static int loc_dup(const SymbolLocList *l, const char *path, int line, int col) {
  if (!l) return 0;
  for (size_t i = 0; i < l->len; i++) {
    if (l->items[i].line == line && l->items[i].col == col &&
        path_same(l->items[i].path, path))
      return 1;
  }
  return 0;
}

static void loc_add(SymbolLocList *l, const char *path, int line, int col,
                    int length, int is_decl, int is_path) {
  if (!l || !path || line < 1 || col < 1 || length < 1) return;
  if (loc_dup(l, path, line, col)) return;
  if (l->len + 1 >= l->cap) {
    size_t ncap = l->cap ? l->cap * 2 : 16;
    SymbolLoc *ni = realloc(l->items, ncap * sizeof(SymbolLoc));
    if (!ni) return;
    l->items = ni;
    l->cap = ncap;
  }
  SymbolLoc *it = &l->items[l->len++];
  memset(it, 0, sizeof(*it));
  it->path = strdup(path);
  it->line = line;
  it->col = col;
  it->length = length;
  it->is_decl = is_decl;
  it->is_path = is_path;
}

static void loc_add_name(SymbolLocList *l, const char *path, const char *text,
                         int line, int hint_col, const char *name, int is_decl,
                         int is_path, int prefer_last) {
  if (!name || !*name) return;
  int col = pick_name_col(text, line > 0 ? line : 1, hint_col, name, prefer_last);
  if (col <= 0) return;
  loc_add(l, path, line > 0 ? line : 1, col, (int)strlen(name), is_decl, is_path);
}

static void walk_file_refs(Node *n, const char *path, const char *text,
                           const char *name, int include_decl,
                           SymbolLocList *out) {
  if (!n) return;

  if (n->type == NODE_COMPONENT_DEF && n->value && name &&
      strcmp(n->value, name) == 0 && n->line > 0) {
    if (include_decl)
      loc_add_name(out, path, text, n->line, n->col, name, 1, 0, 0);
    if (!out->decl_path) {
      out->decl_path = strdup(path);
      out->decl_line = n->line;
    }
  }

  if (n->type == NODE_ELEMENT && n->value && name &&
      strcmp(n->value, name) == 0 && n->line > 0) {
    loc_add_name(out, path, text, n->line, n->col, name, 0, 0, 0);
  }

  if (n->type == NODE_USE) {
    if (n->value2 && name && strcmp(n->value2, name) == 0 && n->line > 0)
      loc_add_name(out, path, text, n->line, n->col, name, 0, 0, 1);
    if (n->value && last_seg_eq(n->value, name) && n->line > 0)
      loc_add_name(out, path, text, n->line, n->col, name, 0, 1, 1);
  }

  if (n->type == NODE_ROUTE && n->value2 && name && n->line > 0) {
    if (!compiler_is_module_ref(n->value2) && strcmp(n->value2, name) == 0)
      loc_add_name(out, path, text, n->line, n->col, name, 0, 0, 1);
    else if (last_seg_eq(n->value2, name))
      loc_add_name(out, path, text, n->line, n->col, name, 0, 1, 1);
  }

  if (n->type == NODE_LAZY_DECL && n->line > 0) {
    if (n->value && name && strcmp(n->value, name) == 0)
      loc_add_name(out, path, text, n->line, n->col, name, 0, 0, 0);
    if (n->value2 && last_seg_eq(n->value2, name))
      loc_add_name(out, path, text, n->line, n->col, name, 0, 1, 1);
  }

  for (size_t i = 0; i < n->children_len; i++)
    walk_file_refs(n->children[i], path, text, name, include_decl, out);
}

static int file_has_component(Node *n, const char *name) {
  if (!n || !name) return 0;
  if (n->type == NODE_COMPONENT_DEF && n->value && strcmp(n->value, name) == 0)
    return 1;
  for (size_t i = 0; i < n->children_len; i++) {
    if (file_has_component(n->children[i], name)) return 1;
  }
  return 0;
}

static void collect_def_names(Node *n, PathList *names) {
  if (!n) return;
  if (n->type == NODE_COMPONENT_DEF && n->value && n->value[0])
    str_list_add(names, n->value);
  for (size_t i = 0; i < n->children_len; i++)
    collect_def_names(n->children[i], names);
}

char *file_text(const char *path, SymbolsOverlayFn overlay, void *ud,
                       int *owned) {
  *owned = 0;
  if (overlay) {
    const char *ov = overlay(path, ud);
    if (ov) return (char *)ov;
  }
  size_t len = 0;
  char *src = fs_read_file(path, &len);
  if (src) *owned = 1;
  return src;
}

void parse_file_into(const char *path, const char *text, const char *name,
                            int include_decl, int want_names, PathList *names,
                            SymbolLocList *refs) {
  if (!path || !text) return;
  CompileResult r = compiler_parse_source(text, strlen(text));
  if (!r.ok || !r.ast || !r.ast->root) {
    compiler_result_free(&r);
    return;
  }
  char *export_name = compiler_module_export_name(path, NULL);
  if (export_name) {
    compiler_wrap_module_body(r.ast->root, export_name, path);
    free(export_name);
  }
  if (want_names && names) collect_def_names(r.ast->root, names);
  if (refs && name) walk_file_refs(r.ast->root, path, text, name, include_decl, refs);
  /* Implicit wrapper (line 0) still records decl_path via walk only when
   * line > 0. Capture basename-equal body-only modules as decl_path. */
  if (refs && name && !refs->decl_path &&
      file_has_component(r.ast->root, name)) {
    refs->decl_path = strdup(path);
    refs->decl_line = 0;
  }
  compiler_result_free(&r);
}

