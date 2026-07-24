#include "application/ports/compiler_port.h"
#include "application/ports/fs_port.h"
#include "adapters/outbound/lexer/lexer.h"
#include "adapters/outbound/parser/parser.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

CompileResult compiler_parse_source(const char *source, size_t source_len) {
  CompileResult result = {0};
  Lexer *lexer = lexer_create(source, source_len);
  Parser *parser = parser_create(lexer);
  AST *ast = parser_parse(parser);

  if (parser->had_error) {
    result.ok = 0;
    result.error = parser->error_msg ? parser->error_msg : "parse error";
    ast_free(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return result;
  }

  result.ok = 1;
  result.ast = ast;
  parser_destroy(parser);
  lexer_destroy(lexer);
  return result;
}

CompileResult compiler_parse_file(const char *path) {
  CompileResult result = {0};
  size_t len = 0;
  char *source = fs_read_file(path, &len);
  if (!source) {
    result.ok = 0;
    result.error = "cannot read file";
    return result;
  }
  result = compiler_parse_source(source, len);
  if (result.ok && result.ast) {
    result.ast->source = source;
  } else {
    free(source);
  }
  return result;
}

/* ── multi-file module resolution ───────────────────────── */

#define MAX_VISITED 128

typedef struct {
  char *paths[MAX_VISITED];
  int count;
} VisitedSet;

static int visited_has(VisitedSet *v, const char *path) {
  for (int i = 0; i < v->count; i++) {
    if (strcmp(v->paths[i], path) == 0) return 1;
  }
  return 0;
}

static void visited_add(VisitedSet *v, const char *path) {
  if (v->count >= MAX_VISITED) return;
  if (visited_has(v, path)) return;
  v->paths[v->count++] = strdup(path);
}

static void visited_free(VisitedSet *v) {
  for (int i = 0; i < v->count; i++) free(v->paths[i]);
  v->count = 0;
}

/* Strip .cord extension and directory → component name */
static char *module_export_name(const char *mod_path, const char *alias) {
  if (alias && *alias) return strdup(alias);
  char *base = fs_basename(mod_path);
  if (!base) return strdup("Module");
  /* remove .cord */
  size_t n = strlen(base);
  if (n > 5 && strcmp(base + n - 5, ".cord") == 0) {
    base[n - 5] = '\0';
  }
  /* default → DefaultLayout for layouts folder hint */
  if (strcmp(base, "default") == 0) {
    free(base);
    return strdup("DefaultLayout");
  }
  /* PascalCase-ish: ensure first letter upper if letter */
  if (base[0] && islower((unsigned char)base[0])) {
    base[0] = (char)toupper((unsigned char)base[0]);
  }
  return base;
}

static int path_is_module_ref(const char *s) {
  if (!s || !*s) return 0;
  return strchr(s, '/') != NULL || strchr(s, '\\') != NULL ||
         strchr(s, '.') != NULL;
}


/* Collapse . and .. in a path. Caller frees. */
static char *path_collapse(const char *path) {
  if (!path) return NULL;
  char *tmp = strdup(path);
  if (!tmp) return NULL;
  for (char *p = tmp; *p; p++) {
    if (*p == '\\') *p = '/';
  }

  int is_abs = (tmp[0] == '/');
  char *stack[128];
  int n = 0;
  char *tok = strtok(tmp, "/");
  while (tok) {
    if (strcmp(tok, ".") == 0) {
      /* skip */
    } else if (strcmp(tok, "..") == 0) {
      if (n > 0) n--;
    } else if (tok[0] != '\0') {
      if (n < 128) stack[n++] = tok;
    }
    tok = strtok(NULL, "/");
  }

  size_t cap = strlen(path) + 4;
  char *out = malloc(cap);
  if (!out) {
    free(tmp);
    return NULL;
  }
  size_t o = 0;
  if (is_abs) out[o++] = '/';
  for (int i = 0; i < n; i++) {
    if (i > 0) out[o++] = '/';
    size_t tl = strlen(stack[i]);
    memcpy(out + o, stack[i], tl);
    o += tl;
  }
  if (n == 0 && !is_abs) {
    out[0] = '.';
    out[1] = '\0';
  } else {
    out[o] = '\0';
  }
  free(tmp);
  return out;
}

/* Absolute collapsed path (joins cwd if relative). Caller frees. */
static char *path_make_abs(const char *path) {
  if (!path) return NULL;
  int is_abs = (path[0] == '/');
#ifdef _WIN32
  if (path[0] && path[1] == ':') is_abs = 1;
#endif
  if (is_abs) return path_collapse(path);

  char *cwd = fs_cwd();
  if (!cwd) return path_collapse(path);
  size_t lc = strlen(cwd), lp = strlen(path);
  char *joined = malloc(lc + lp + 2);
  if (!joined) {
    free(cwd);
    return path_collapse(path);
  }
  memcpy(joined, cwd, lc);
  joined[lc] = '/';
  memcpy(joined + lc + 1, path, lp + 1);
  free(cwd);
  char *out = path_collapse(joined);
  free(joined);
  return out;
}

/* True if path is equal to root or a descendant (after abs collapse). */
static int path_is_under_root(const char *path, const char *root) {
  if (!path || !root || !*root) return 0;
  char *ap = path_make_abs(path);
  char *ar = path_make_abs(root);
  if (!ap || !ar) {
    free(ap);
    free(ar);
    return 0;
  }
  size_t rl = strlen(ar);
  int ok = 0;
  if (strncmp(ap, ar, rl) == 0) {
    if (ap[rl] == '\0' || ap[rl] == '/') ok = 1;
  }
  free(ap);
  free(ar);
  return ok;
}

/* Directory with cordlang.json walking up from entry, else entry dirname. */
static char *find_project_root(const char *entry_path) {
  char *start = fs_dirname(entry_path);
  char *cur = path_make_abs(start);
  free(start);
  if (!cur) return NULL;
  char *fallback = strdup(cur);
  for (int depth = 0; depth < 48; depth++) {
    size_t n = strlen(cur);
    char *cfg = malloc(n + 20);
    if (!cfg) break;
    memcpy(cfg, cur, n);
    memcpy(cfg + n, "/cordlang.json", 15);
    int found = fs_exists(cfg);
    free(cfg);
    if (found) {
      free(fallback);
      return cur;
    }
    if (strcmp(cur, "/") == 0) break;
#ifdef _WIN32
    if (strlen(cur) <= 3 && cur[1] == ':') break;
#endif
    char *parent = fs_dirname(cur);
    char *next = path_make_abs(parent);
    free(parent);
    if (!next || strcmp(next, cur) == 0) {
      free(next);
      break;
    }
    free(cur);
    cur = next;
  }
  free(cur);
  return fallback;
}

static char *resolve_module_file(const char *from_file, const char *mod_path,
                                 const char *project_root) {
  if (!mod_path || !*mod_path) return NULL;

  char *dir = fs_dirname(from_file);
  if (!dir) return NULL;

  /* join dir + mod_path with / */
  size_t ld = strlen(dir), lm = strlen(mod_path);
  char *joined = malloc(ld + lm + 2);
  if (!joined) {
    free(dir);
    return NULL;
  }
  memcpy(joined, dir, ld);
  joined[ld] = '/';
  memcpy(joined + ld + 1, mod_path, lm + 1);
  free(dir);

  char *collapsed = path_collapse(joined);
  free(joined);
  if (!collapsed) return NULL;

  size_t n = strlen(collapsed);
  int has_ext = (n > 5 && strcmp(collapsed + n - 5, ".cord") == 0);

  char *candidate = NULL;
  if (fs_exists(collapsed) && !fs_is_dir(collapsed)) {
    candidate = collapsed;
    collapsed = NULL;
  } else if (!has_ext) {
    char *with_ext = malloc(n + 6);
    if (with_ext) {
      memcpy(with_ext, collapsed, n);
      memcpy(with_ext + n, ".cord", 6);
      if (fs_exists(with_ext) && !fs_is_dir(with_ext)) {
        candidate = with_ext;
      } else {
        free(with_ext);
      }
    }
  }
  free(collapsed);

  if (!candidate) return NULL;

  /* Jail: module must stay under project root */
  if (project_root && !path_is_under_root(candidate, project_root)) {
    free(candidate);
    return NULL;
  }
  return candidate;
}

/* Move all children from src root into dst root (steal pointers) */
static void merge_ast_children(Node *dst_root, Node *src_root) {
  if (!dst_root || !src_root) return;
  for (size_t i = 0; i < src_root->children_len; i++) {
    Node *c = src_root->children[i];
    if (!c) continue;
    if (c->type == NODE_USE) {
      /* uses resolved separately; drop from merge of raw file later */
      continue;
    }
    src_root->children[i] = NULL;
    node_add_child(dst_root, c);
  }
  src_root->children_len = 0;
}

/* Attach source file path on COMPONENT_DEF for symbols/goto (C7). */
static int component_has_file_attr(Node *def) {
  if (!def) return 0;
  for (size_t i = 0; i < def->children_len; i++) {
    Node *c = def->children[i];
    if (c && c->type == NODE_ATTR && c->value &&
        strcmp(c->value, "__file__") == 0)
      return 1;
  }
  return 0;
}

static void tag_component_file(Node *def, const char *file_path) {
  if (!def || def->type != NODE_COMPONENT_DEF || !file_path) return;
  if (component_has_file_attr(def)) return;
  Node *attr = node_create(NODE_ATTR, "__file__", def->line, def->col);
  /* Normalize separators for stable display */
  char *norm = fs_norm_path(file_path);
  attr->value2 = norm ? norm : strdup(file_path);
  node_add_child(def, attr);
}

static void tag_components_in_root(Node *root, const char *file_path) {
  if (!root || !file_path) return;
  for (size_t i = 0; i < root->children_len; i++) {
    Node *c = root->children[i];
    if (c && c->type == NODE_COMPONENT_DEF)
      tag_component_file(c, file_path);
  }
}

/*
 * If a file has no def/layout and only UI/state/props, wrap as component
 * named export_name. Layouts folder files → layout component.
 */
static void ensure_component_wrapper(Node *root, const char *export_name,
                                     const char *file_path) {
  if (!root || !export_name) return;

  /* Already exports this name (explicit def Foo) — leave as-is */
  for (size_t i = 0; i < root->children_len; i++) {
    Node *c = root->children[i];
    if (c && c->type == NODE_COMPONENT_DEF && c->value &&
        strcmp(c->value, export_name) == 0)
      return;
  }

  int has_ui = 0;
  for (size_t i = 0; i < root->children_len; i++) {
    Node *c = root->children[i];
    if (!c) continue;
    if (c->type == NODE_ELEMENT || c->type == NODE_FOR || c->type == NODE_IF ||
        c->type == NODE_STATE_DECL || c->type == NODE_PROPS_DECL ||
        c->type == NODE_COMPUTED_DECL || c->type == NODE_SLOT ||
        c->type == NODE_HEAD || c->type == NODE_LOADING ||
        c->type == NODE_EMPTY || c->type == NODE_SUSPENSE ||
        c->type == NODE_ERROR_BOUNDARY || c->type == NODE_ACTION_DECL ||
        c->type == NODE_FETCH_DECL || c->type == NODE_EFFECT_DECL ||
        c->type == NODE_REF_DECL || c->type == NODE_PARAMS_DECL ||
        c->type == NODE_AWAIT || c->type == NODE_SNIPPET ||
        c->type == NODE_STORE_DECL || c->type == NODE_RENDER ||
        c->type == NODE_PORTAL || c->type == NODE_INSERTION_EFFECT ||
        c->type == NODE_EFFECT_EVENT || c->type == NODE_EXTERNAL_STORE ||
        c->type == NODE_IMPERATIVE_HANDLE || c->type == NODE_LAYOUT_EFFECT)
      has_ui = 1;
  }
  if (!has_ui) return;

  int is_layout_file = 0;
  if (file_path &&
      (strstr(file_path, "layout") || strstr(file_path, "Layout")))
    is_layout_file = 1;

  Node *def = node_create(NODE_COMPONENT_DEF, export_name, 0, 0);
  if (is_layout_file) def->value2 = strdup("__layout__");

  /* Steal UI/state/props into def — keep imported COMPONENT_DEFs as siblings */
  for (size_t i = 0; i < root->children_len; i++) {
    Node *c = root->children[i];
    if (!c) continue;
    if (c->type == NODE_USE || c->type == NODE_ROUTE || c->type == NODE_THEME ||
        c->type == NODE_COMPONENT_DEF || c->type == NODE_CONTEXT_DECL)
      continue;
    root->children[i] = NULL;
    node_add_child(def, c);
  }
  {
    size_t w = 0;
    for (size_t i = 0; i < root->children_len; i++) {
      if (root->children[i]) root->children[w++] = root->children[i];
    }
    root->children_len = w;
  }
  node_add_child(root, def);
}

static int load_module_into(Node *dst_root, const char *from_file,
                            const char *mod_path, const char *alias,
                            VisitedSet *visited, char *errbuf, size_t errlen,
                            const char *project_root);

/* Resolve NODE_USE and route module paths in this AST (from_file context) */
static int resolve_uses_in_ast(Node *root, const char *from_file,
                               VisitedSet *visited, char *errbuf,
                               size_t errlen, const char *project_root) {
  if (!root) return 0;

  /* Collect uses first (indices may shift) */
  for (size_t i = 0; i < root->children_len; i++) {
    Node *c = root->children[i];
    if (!c) continue;

    if (c->type == NODE_USE) {
      const char *mod = c->value;
      const char *alias = c->value2;
      if (!mod || !*mod) {
        snprintf(errbuf, errlen, "empty use path in %s", from_file);
        return -1;
      }
      if (load_module_into(root, from_file, mod, alias, visited, errbuf,
                           errlen, project_root) != 0)
        return -1;
      /* remove USE node */
      node_free(c);
      root->children[i] = NULL;
    } else if (c->type == NODE_ROUTE && c->value2 &&
               path_is_module_ref(c->value2)) {
      /* route / => pages/HomePage — load module, rewrite to component name */
      char *export_name = module_export_name(c->value2, NULL);
      if (load_module_into(root, from_file, c->value2, export_name, visited,
                           errbuf, errlen, project_root) != 0) {
        free(export_name);
        return -1;
      }
      free(c->value2);
      c->value2 = export_name;
    } else if (c->type == NODE_LAZY_DECL && c->value2 &&
               path_is_module_ref(c->value2)) {
      /* lazy ProductPage = pages/ProductPage — load module for codegen */
      char *export_name = module_export_name(c->value2, c->value);
      if (load_module_into(root, from_file, c->value2, export_name, visited,
                           errbuf, errlen, project_root) != 0) {
        free(export_name);
        return -1;
      }
      /* keep NODE_LAZY_DECL; normalize name if needed */
      if (c->value) free(c->value);
      c->value = export_name;
    }
  }

  /* compact NULLs */
  size_t w = 0;
  for (size_t i = 0; i < root->children_len; i++) {
    if (root->children[i]) root->children[w++] = root->children[i];
  }
  root->children_len = w;
  return 0;
}

static int load_module_into(Node *dst_root, const char *from_file,
                            const char *mod_path, const char *alias,
                            VisitedSet *visited, char *errbuf, size_t errlen,
                            const char *project_root) {
  char *resolved = resolve_module_file(from_file, mod_path, project_root);
  if (!resolved) {
    snprintf(errbuf, errlen,
             "cannot resolve module '%s' (from %s) — missing or outside project",
             mod_path, from_file);
    return -1;
  }

  if (visited_has(visited, resolved)) {
    free(resolved);
    return 0; /* already loaded */
  }
  visited_add(visited, resolved);

  CompileResult sub = compiler_parse_file(resolved);
  if (!sub.ok || !sub.ast) {
    snprintf(errbuf, errlen, "failed to parse module '%s'%s%s", resolved,
             sub.error ? ": " : "", sub.error ? sub.error : "");
    free(resolved);
    return -1;
  }

  char *export_name = module_export_name(mod_path, alias);

  /* Resolve nested uses relative to this module file */
  if (resolve_uses_in_ast(sub.ast->root, resolved, visited, errbuf, errlen,
                          project_root) != 0) {
    free(export_name);
    free(resolved);
    compiler_result_free(&sub);
    return -1;
  }

  ensure_component_wrapper(sub.ast->root, export_name, resolved);

  /* Rename single anonymous-style def if file had def with different name? 
   * If file has `def Foo` keep it. Wrapper already handled body-only files.
   * If alias provided and single COMPONENT_DEF, rename. */
  if (alias && *alias) {
    int n_defs = 0;
    Node *only = NULL;
    for (size_t i = 0; i < sub.ast->root->children_len; i++) {
      Node *c = sub.ast->root->children[i];
      if (c && c->type == NODE_COMPONENT_DEF) {
        n_defs++;
        only = c;
      }
    }
    if (n_defs == 1 && only) {
      free(only->value);
      only->value = strdup(alias);
    }
  }

  /* C7: record defining file for symbols / goto */
  tag_components_in_root(sub.ast->root, resolved);

  merge_ast_children(dst_root, sub.ast->root);

  free(export_name);
  free(resolved);
  compiler_result_free(&sub);
  return 0;
}

CompileResult compiler_parse_project(const char *entry_path) {
  CompileResult result = {0};
  static char errbuf[512];
  errbuf[0] = '\0';

  if (!entry_path) {
    result.ok = 0;
    result.error = "no entry path";
    return result;
  }

  result = compiler_parse_file(entry_path);
  if (!result.ok || !result.ast) return result;

  char *project_root = find_project_root(entry_path);
  VisitedSet visited = {0};
  visited_add(&visited, entry_path);

  if (resolve_uses_in_ast(result.ast->root, entry_path, &visited, errbuf,
                          sizeof(errbuf), project_root) != 0) {
    compiler_result_free(&result);
    result.ok = 0;
    result.ast = NULL;
    result.error = errbuf[0] ? errbuf : "module resolution failed";
    visited_free(&visited);
    free(project_root);
    return result;
  }

  /* Tag any component defs that live in the entry file itself */
  tag_components_in_root(result.ast->root, entry_path);

  visited_free(&visited);
  free(project_root);
  return result;
}

void compiler_result_free(CompileResult *result) {
  if (!result) return;
  if (result->ast) {
    ast_free(result->ast);
    result->ast = NULL;
  }
}
