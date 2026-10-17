#include "adapters/outbound/compiler/compiler_internal.h"
#include "adapters/outbound/lexer/lexer.h"
#include "adapters/outbound/parser/parser.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void merge_ast_children(Node *dst_root, Node *src_root) {
  if (!dst_root || !src_root) return;
  for (size_t i = 0; i < src_root->children_len; i++) {
    Node *c = src_root->children[i];
    if (!c) continue;
    if (c->type == NODE_USE) {
      /* uses already resolved separately; drop and free so ast_free of
       * the submodule does not lose them when children_len is cleared. */
      src_root->children[i] = NULL;
      node_free(c);
      continue;
    }
    src_root->children[i] = NULL;
    node_add_child(dst_root, c);
  }
  src_root->children_len = 0;
}

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

void tag_components_in_root(Node *root, const char *file_path) {
  if (!root || !file_path) return;
  for (size_t i = 0; i < root->children_len; i++) {
    Node *c = root->children[i];
    if (c && c->type == NODE_COMPONENT_DEF)
      tag_component_file(c, file_path);
  }
}

void ensure_component_wrapper(Node *root, const char *export_name,
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

int resolve_uses_in_ast(Node *root, const char *from_file,
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

int load_module_into(Node *dst_root, const char *from_file,
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
