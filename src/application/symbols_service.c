#include "application/symbols_service.h"
#include "application/ports/compiler_port.h"
#include "application/ports/fs_port.h"
#include "adapters/outbound/json/json_mini.h"
#include "domain/ast.h"
#include "domain/known_attrs.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define STRICMP _stricmp
#else
#define STRICMP strcasecmp
#endif

static char *read_entry_from_config(const char *project_dir) {
  char *cfg = fs_join(project_dir, "cordlang.json");
  if (!cfg) return NULL;

  size_t len = 0;
  char *json = fs_read_file(cfg, &len);
  free(cfg);
  if (!json) return NULL;

  char *entry = json_object_get_string(json, "entry");
  free(json);
  if (!entry) entry = strdup("src/app.cord");
  return entry;
}

/* Prefer paths relative to project (e.g. src/components/Counter.cord). */
static char *display_path(const char *file, const char *project_dir) {
  if (!file) return strdup("?");
  char *norm = fs_norm_path(file);
  const char *p = norm ? norm : file;

  /* Strip project_dir prefix if present */
  if (project_dir && *project_dir) {
    char *pnorm = fs_norm_path(project_dir);
    if (pnorm) {
      size_t pl = strlen(pnorm);
      if (pl > 0 && strncmp(p, pnorm, pl) == 0) {
        const char *rest = p + pl;
        while (*rest == '/' || *rest == '\\') rest++;
        if (*rest) {
          char *out = strdup(rest);
          free(pnorm);
          free(norm);
          return out;
        }
      }
      free(pnorm);
    }
  }

  /* If path contains src/, show from there */
  const char *src = strstr(p, "src/");
  if (!src) {
#ifdef _WIN32
    src = strstr(p, "src\\");
#endif
  }
  if (src) {
    char *out = strdup(src);
    free(norm);
    /* normalize backslashes */
    for (char *c = out; *c; c++)
      if (*c == '\\') *c = '/';
    return out;
  }

  char *out = strdup(p);
  free(norm);
  if (out) {
    for (char *c = out; *c; c++)
      if (*c == '\\') *c = '/';
  }
  return out ? out : strdup(file);
}

static const char *component_file(Node *def) {
  if (!def) return NULL;
  for (size_t i = 0; i < def->children_len; i++) {
    Node *c = def->children[i];
    if (c && c->type == NODE_ATTR && c->value &&
        strcmp(c->value, "__file__") == 0 && c->value2)
      return c->value2;
  }
  return NULL;
}

static int is_layout_def(Node *def) {
  return def && def->type == NODE_COMPONENT_DEF && def->value2 &&
         strcmp(def->value2, "__layout__") == 0;
}

static int resolve_entry(const char *project_dir, const char *entry_path,
                         char **out_entry) {
  const char *dir = project_dir && *project_dir ? project_dir : ".";
  if (entry_path && *entry_path) {
    if (fs_exists(entry_path)) {
      *out_entry = strdup(entry_path);
      return 0;
    }
    char *joined = fs_join(dir, entry_path);
    if (joined && fs_exists(joined)) {
      *out_entry = joined;
      return 0;
    }
    free(joined);
    fprintf(stderr, "Error: entry not found: %s\n", entry_path);
    return 1;
  }

  char *cfg = fs_join(dir, "cordlang.json");
  if (cfg && fs_exists(cfg)) {
    free(cfg);
    char *rel = read_entry_from_config(dir);
    char *joined = fs_join(dir, rel ? rel : "src/app.cord");
    free(rel);
    if (joined && fs_exists(joined)) {
      *out_entry = joined;
      return 0;
    }
    free(joined);
  } else {
    free(cfg);
  }

  /* bare .cord file as project dir? */
  if (entry_path && strstr(entry_path, ".cord")) {
    fprintf(stderr, "Error: entry not found: %s\n", entry_path);
    return 1;
  }

  fprintf(stderr, "Error: not a Cordlang project (missing cordlang.json)\n");
  fprintf(stderr, "Usage: cordlang symbols [entry.cord]\n");
  return 1;
}

int symbols_service_list(const char *project_dir, const char *entry_path) {
  char *entry = NULL;
  if (resolve_entry(project_dir, entry_path, &entry) != 0) return 1;

  CompileResult r = compiler_parse_project(entry);
  if (!r.ok || !r.ast || !r.ast->root) {
    fprintf(stderr, "Error: parse failed%s%s\n", r.error ? ": " : "",
            r.error ? r.error : "");
    free(entry);
    compiler_result_free(&r);
    return 1;
  }

  const char *dir = project_dir && *project_dir ? project_dir : ".";
  Node *root = r.ast->root;

  /* Components (non-layout) */
  for (size_t i = 0; i < root->children_len; i++) {
    Node *c = root->children[i];
    if (!c || c->type != NODE_COMPONENT_DEF || !c->value) continue;
    if (is_layout_def(c)) continue;
    const char *file = component_file(c);
    char *disp = display_path(file ? file : entry, dir);
    int line = c->line > 0 ? c->line : 1;
    printf("COMPONENT %s  %s:%d\n", c->value, disp, line);
    free(disp);
  }

  /* Routes */
  for (size_t i = 0; i < root->children_len; i++) {
    Node *c = root->children[i];
    if (!c || c->type != NODE_ROUTE) continue;
    const char *path = c->value ? c->value : "?";
    const char *target = c->value2 ? c->value2 : "?";
    printf("ROUTE %s => %s\n", path, target);
  }

  /* Layouts */
  for (size_t i = 0; i < root->children_len; i++) {
    Node *c = root->children[i];
    if (!c || c->type != NODE_COMPONENT_DEF || !c->value) continue;
    if (!is_layout_def(c)) continue;
    printf("LAYOUT %s\n", c->value);
  }

  compiler_result_free(&r);
  free(entry);
  return 0;
}

int symbols_service_goto(const char *project_dir, const char *name,
                         const char *entry_path) {
  if (!name || !*name) {
    fprintf(stderr, "Error: missing symbol name\n");
    fprintf(stderr, "Usage: cordlang goto <ComponentName>\n");
    return 1;
  }

  char *entry = NULL;
  if (resolve_entry(project_dir, entry_path, &entry) != 0) return 1;

  CompileResult r = compiler_parse_project(entry);
  if (!r.ok || !r.ast || !r.ast->root) {
    fprintf(stderr, "Error: parse failed%s%s\n", r.error ? ": " : "",
            r.error ? r.error : "");
    free(entry);
    compiler_result_free(&r);
    return 1;
  }

  const char *dir = project_dir && *project_dir ? project_dir : ".";
  Node *root = r.ast->root;
  int found = 0;

  for (size_t i = 0; i < root->children_len; i++) {
    Node *c = root->children[i];
    if (!c || c->type != NODE_COMPONENT_DEF || !c->value) continue;
    if (strcmp(c->value, name) != 0) continue;

    const char *file = component_file(c);
    char *disp = display_path(file ? file : entry, dir);
    int line = c->line > 0 ? c->line : 1;
    printf("%s:%d\n", disp, line);
    free(disp);
    found = 1;
    break;
  }

  /* Also match route target names that might only exist as rewritten exports */
  if (!found) {
    for (size_t i = 0; i < root->children_len; i++) {
      Node *c = root->children[i];
      if (!c || c->type != NODE_ROUTE || !c->value2) continue;
      if (strcmp(c->value2, name) != 0) continue;
      /* Find matching component (should have been loaded) */
      for (size_t j = 0; j < root->children_len; j++) {
        Node *d = root->children[j];
        if (d && d->type == NODE_COMPONENT_DEF && d->value &&
            strcmp(d->value, name) == 0) {
          const char *file = component_file(d);
          char *disp = display_path(file ? file : entry, dir);
          int line = d->line > 0 ? d->line : 1;
          printf("%s:%d\n", disp, line);
          free(disp);
          found = 1;
          break;
        }
      }
      if (found) break;
    }
  }

  compiler_result_free(&r);
  free(entry);

  if (!found) {
    fprintf(stderr, "Error: symbol '%s' not found\n", name);
    return 1;
  }
  return 0;
}

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

static int ident_char(int c) {
  return isalnum((unsigned char)c) || c == '_';
}

int symbols_valid_component_name(const char *name) {
  static const char *reserved[] = {
      "def",     "state",  "props",    "computed", "route",  "layout",
      "theme",   "use",    "import",   "as",       "from",   "effect",
      "ref",     "ctx",    "provide",  "params",   "fetch",  "lazy",
      "if",      "else",   "for",      "in",       "key",    "slot",
      "foreign", "head",   "loading",  "empty",    "await",  "snippet",
      "render",  "portal", "suspense", "store",    NULL};
  if (!name || !*name) return 0;
  if (!(isalpha((unsigned char)name[0]) || name[0] == '_')) return 0;
  for (const char *p = name + 1; *p; p++) {
    if (!ident_char((unsigned char)*p)) return 0;
  }
  for (int i = 0; reserved[i]; i++) {
    if (strcmp(name, reserved[i]) == 0) return 0;
  }
  if (cord_is_builtin_tag(name)) return 0;
  if (cord_is_jsx_hook_name(name)) return 0;
  return 1;
}

static int path_same(const char *a, const char *b) {
  if (!a || !b) return 0;
  char *na = fs_norm_path(a);
  char *nb = fs_norm_path(b);
  int eq = 0;
  if (na && nb) {
#ifdef _WIN32
    eq = STRICMP(na, nb) == 0;
#else
    eq = strcmp(na, nb) == 0;
#endif
  }
  free(na);
  free(nb);
  return eq;
}

typedef struct {
  char **paths;
  size_t n, cap;
} PathList;

static void str_list_add(PathList *pl, const char *s) {
  if (!pl || !s || !*s) return;
  for (size_t i = 0; i < pl->n; i++) {
    if (strcmp(pl->paths[i], s) == 0) return;
  }
  if (pl->n + 1 >= pl->cap) {
    size_t ncap = pl->cap ? pl->cap * 2 : 16;
    char **np = realloc(pl->paths, ncap * sizeof(char *));
    if (!np) return;
    pl->paths = np;
    pl->cap = ncap;
  }
  pl->paths[pl->n++] = strdup(s);
}

static void path_list_add(PathList *pl, const char *path) {
  if (!pl || !path || !*path) return;
  for (size_t i = 0; i < pl->n; i++) {
    if (path_same(pl->paths[i], path)) return;
  }
  if (pl->n + 1 >= pl->cap) {
    size_t ncap = pl->cap ? pl->cap * 2 : 16;
    char **np = realloc(pl->paths, ncap * sizeof(char *));
    if (!np) return;
    pl->paths = np;
    pl->cap = ncap;
  }
  pl->paths[pl->n++] = strdup(path);
}

static void path_list_free(PathList *pl) {
  if (!pl) return;
  for (size_t i = 0; i < pl->n; i++) free(pl->paths[i]);
  free(pl->paths);
  memset(pl, 0, sizeof(*pl));
}

static void walk_add_path(const char *abs, const char *rel, void *ud) {
  (void)rel;
  path_list_add((PathList *)ud, abs);
}

static const char *line_at(const char *text, int line_1, size_t *out_len) {
  if (!text || line_1 < 1) return NULL;
  int ln = 1;
  const char *p = text;
  while (*p && ln < line_1) {
    if (*p == '\n') ln++;
    p++;
  }
  if (ln != line_1) return NULL;
  const char *start = p;
  while (*p && *p != '\n' && *p != '\r') p++;
  if (out_len) *out_len = (size_t)(p - start);
  return start;
}

/* 0-based columns of word-boundary `name` on a line, skipping strings. */
static int line_name_cols(const char *line, size_t llen, const char *name,
                          int *cols, int max_cols) {
  if (!line || !name || !cols || max_cols <= 0) return 0;
  size_t nlen = strlen(name);
  if (nlen == 0) return 0;
  int n = 0;
  int in_str = 0;
  for (size_t i = 0; i < llen;) {
    char c = line[i];
    if (!in_str && c == '"') {
      in_str = 1;
      i++;
      continue;
    }
    if (in_str) {
      if (c == '\\' && i + 1 < llen) {
        i += 2;
        continue;
      }
      if (c == '"') in_str = 0;
      i++;
      continue;
    }
    if (c == '#' && (i + 1 >= llen || line[i + 1] != '{')) break;
    if (c == '/' && i + 1 < llen && line[i + 1] == '/') break;
    if (i + nlen <= llen && strncmp(line + i, name, nlen) == 0) {
      int before = (i == 0) || !ident_char((unsigned char)line[i - 1]);
      int after =
          (i + nlen >= llen) || !ident_char((unsigned char)line[i + nlen]);
      if (before && after && n < max_cols) cols[n++] = (int)i;
      i += nlen;
      continue;
    }
    i++;
  }
  return n;
}

/* Pick a 1-based start column for `name` on `line_1`. hint_col_1 is the AST
 * token column (lexer stores the exclusive end). prefer_last for path segs. */
static int pick_name_col(const char *text, int line_1, int hint_col_1,
                         const char *name, int prefer_last) {
  size_t llen = 0;
  const char *line = line_at(text, line_1, &llen);
  if (!line || !name) return 0;
  int cols[16];
  int n = line_name_cols(line, llen, name, cols, 16);
  if (n <= 0) return 0;
  if (n == 1) return cols[0] + 1;
  size_t nlen = strlen(name);
  int hint0 = hint_col_1 > 0 ? hint_col_1 - 1 : 0;
  int best = prefer_last ? cols[n - 1] : cols[0];
  int best_d = 100000;
  for (int i = 0; i < n; i++) {
    int end0 = cols[i] + (int)nlen;
    int d = abs(end0 - hint0);
    int d2 = abs(cols[i] - hint0);
    if (d2 < d) d = d2;
    if (d < best_d) {
      best_d = d;
      best = cols[i];
    }
  }
  return best + 1;
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

static char *file_text(const char *path, SymbolsOverlayFn overlay, void *ud,
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

static void parse_file_into(const char *path, const char *text, const char *name,
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

static char *project_root_of(const char *anchor_path) {
  if (!anchor_path || !*anchor_path) return fs_cwd();
  char *root = compiler_project_root(anchor_path);
  if (root) return root;
  return fs_dirname(anchor_path);
}

static void gather_cord_files(const char *anchor_path, const char **extra,
                              int n_extra, PathList *pl) {
  char *root = project_root_of(anchor_path);
  if (root) {
    fs_walk_cord(root, walk_add_path, pl);
    free(root);
  }
  if (anchor_path) path_list_add(pl, anchor_path);
  for (int i = 0; i < n_extra; i++) {
    if (extra[i]) path_list_add(pl, extra[i]);
  }
}

static void scan_project(const char *anchor_path, const char *name,
                         int include_decl, SymbolsOverlayFn overlay, void *ud,
                         const char **extra_paths, int n_extra, PathList *names,
                         SymbolLocList *refs) {
  PathList files = {0};
  gather_cord_files(anchor_path, extra_paths, n_extra, &files);
  for (size_t i = 0; i < files.n; i++) {
    int owned = 0;
    char *text = file_text(files.paths[i], overlay, ud, &owned);
    if (!text) continue;
    parse_file_into(files.paths[i], text, name, include_decl, names != NULL,
                    names, refs);
    if (owned) free(text);
  }
  path_list_free(&files);
}

int symbols_name_is_component(const char *anchor_path, const char *name,
                              SymbolsOverlayFn overlay, void *ud) {
  if (!name || !*name) return 0;
  PathList names = {0};
  scan_project(anchor_path, NULL, 1, overlay, ud, NULL, 0, &names, NULL);
  int ok = 0;
  for (size_t i = 0; i < names.n; i++) {
    if (strcmp(names.paths[i], name) == 0) {
      ok = 1;
      break;
    }
  }
  path_list_free(&names);
  return ok;
}

int symbols_collect_refs(const char *anchor_path, const char *name,
                         int include_decl, SymbolsOverlayFn overlay, void *ud,
                         const char **extra_paths, int n_extra,
                         SymbolLocList *out) {
  if (!out) return 1;
  symbol_loc_list_init(out);
  if (!name || !*name) return 1;
  scan_project(anchor_path, name, include_decl, overlay, ud, extra_paths,
               n_extra, NULL, out);
  return 0;
}

int symbols_rename_check(const char *anchor_path, const char *old_name,
                         const char *new_name, SymbolsOverlayFn overlay,
                         void *ud, char *msg_buf, size_t msg_sz) {
  if (msg_buf && msg_sz) msg_buf[0] = '\0';
  if (!new_name || !symbols_valid_component_name(new_name)) {
    if (msg_buf && msg_sz)
      snprintf(msg_buf, msg_sz, "invalid component name '%s'",
               new_name ? new_name : "");
    return 1;
  }
  if (old_name && strcmp(old_name, new_name) == 0) return 0;
  PathList names = {0};
  scan_project(anchor_path, NULL, 1, overlay, ud, NULL, 0, &names, NULL);
  int collide = 0;
  for (size_t i = 0; i < names.n; i++) {
    if (strcmp(names.paths[i], new_name) == 0) {
      collide = 1;
      break;
    }
  }
  path_list_free(&names);
  if (collide) {
    if (msg_buf && msg_sz)
      snprintf(msg_buf, msg_sz, "component '%s' already exists", new_name);
    return 2;
  }
  return 0;
}
