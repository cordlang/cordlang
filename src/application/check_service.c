#include "application/check_service.h"

#include "application/ports/compiler_port.h"
#include "domain/ast.h"
#include "domain/known_attrs.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_NAMES 256
#define MAX_ROUTES 128
#define MAX_CONTEXTS 64
#define NAME_LEN 96

typedef struct {
  char names[MAX_NAMES][NAME_LEN];
  int count;
  int lines[MAX_NAMES];
  int cols[MAX_NAMES];
} NameSet;

typedef struct {
  char path[256];
  char target[NAME_LEN];
  int line, col;
} RouteInfo;

typedef struct {
  RouteInfo routes[MAX_ROUTES];
  int n_routes;
  char contexts[MAX_CONTEXTS][NAME_LEN];
  int n_contexts;
} CheckCtx;

static int is_pascal_case(const char *s) {
  return s && s[0] && isupper((unsigned char)s[0]);
}

static int name_set_has(const NameSet *s, const char *name) {
  if (!s || !name) return 0;
  for (int i = 0; i < s->count; i++) {
    if (strcmp(s->names[i], name) == 0) return 1;
  }
  return 0;
}

static int name_set_add(NameSet *s, const char *name, int line, int col) {
  if (!s || !name || !*name) return 0;
  if (s->count >= MAX_NAMES) return 0;
  if (name_set_has(s, name)) return 0;
  strncpy(s->names[s->count], name, NAME_LEN - 1);
  s->names[s->count][NAME_LEN - 1] = '\0';
  s->lines[s->count] = line;
  s->cols[s->count] = col;
  s->count++;
  return 1;
}

static int ctx_has_context(const CheckCtx *c, const char *name) {
  if (!c || !name) return 0;
  for (int i = 0; i < c->n_contexts; i++) {
    if (strcmp(c->contexts[i], name) == 0) return 1;
  }
  return 0;
}

static void ctx_add_context(CheckCtx *c, const char *name) {
  if (!c || !name || !*name || c->n_contexts >= MAX_CONTEXTS) return;
  if (ctx_has_context(c, name)) return;
  strncpy(c->contexts[c->n_contexts], name, NAME_LEN - 1);
  c->contexts[c->n_contexts][NAME_LEN - 1] = '\0';
  c->n_contexts++;
}

/* Extract :param names from a route path into names[] (comma-free tokens). */
static int extract_route_params(const char *path, char names[][NAME_LEN],
                                int max_names) {
  if (!path) return 0;
  int n = 0;
  const char *p = path;
  while (*p && n < max_names) {
    if (*p == ':') {
      p++;
      size_t i = 0;
      while (*p && *p != '/' && *p != '?' && *p != '#' &&
             !isspace((unsigned char)*p) && i + 1 < NAME_LEN) {
        names[n][i++] = *p++;
      }
      names[n][i] = '\0';
      if (i > 0) n++;
    } else {
      p++;
    }
  }
  return n;
}

static void collect_defs(Node *n, NameSet *defs, NameSet *dup_report,
                         DiagList *out, const char *file) {
  if (!n) return;

  if (n->type == NODE_COMPONENT_DEF && n->value && n->value[0]) {
    if (name_set_has(defs, n->value)) {
      diag_emit(out, DIAG_ERROR, file, n->line, n->col,
                "duplicate component name '%s'", n->value);
      name_set_add(dup_report, n->value, n->line, n->col);
    } else {
      name_set_add(defs, n->value, n->line, n->col);
    }
  }

  /* lazy Foo = ... also introduces a component name */
  if (n->type == NODE_LAZY_DECL && n->value && n->value[0]) {
    if (!name_set_has(defs, n->value))
      name_set_add(defs, n->value, n->line, n->col);
  }

  if (n->type == NODE_CONTEXT_DECL && n->value && n->value[0]) {
    /* collected separately in CheckCtx via walk */
  }

  for (size_t i = 0; i < n->children_len; i++)
    collect_defs(n->children[i], defs, dup_report, out, file);
}

static void collect_contexts_and_routes(Node *n, CheckCtx *ctx) {
  if (!n) return;

  if (n->type == NODE_CONTEXT_DECL && n->value && n->value[0])
    ctx_add_context(ctx, n->value);

  if (n->type == NODE_ROUTE && ctx->n_routes < MAX_ROUTES) {
    RouteInfo *r = &ctx->routes[ctx->n_routes];
    r->path[0] = '\0';
    r->target[0] = '\0';
    if (n->value) {
      strncpy(r->path, n->value, sizeof(r->path) - 1);
      r->path[sizeof(r->path) - 1] = '\0';
    }
    if (n->value2) {
      strncpy(r->target, n->value2, sizeof(r->target) - 1);
      r->target[sizeof(r->target) - 1] = '\0';
    }
    r->line = n->line;
    r->col = n->col;
    ctx->n_routes++;
  }

  for (size_t i = 0; i < n->children_len; i++)
    collect_contexts_and_routes(n->children[i], ctx);
}

static void check_prop_types(Node *n, DiagList *out, const char *file) {
  if (!n) return;
  if (n->type == NODE_PROPS_DECL) {
    for (size_t i = 0; i < n->children_len; i++) {
      Node *ch = n->children[i];
      if (!ch) continue;
      for (size_t j = 0; j < ch->children_len; j++) {
        Node *a = ch->children[j];
        if (!a || a->type != NODE_ATTR || !a->value) continue;
        if (strcmp(a->value, "type") != 0) continue;
        if (!a->value2 || !cord_is_prop_type(a->value2)) {
          diag_emit(out, DIAG_ERROR, file, a->line, a->col,
                    "unknown prop type '%s' on '%s' (use string|number|boolean|any)",
                    a->value2 ? a->value2 : "?",
                    ch->value ? ch->value : "?");
        }
      }
    }
  }
  for (size_t i = 0; i < n->children_len; i++)
    check_prop_types(n->children[i], out, file);
}

/* Find enclosing COMPONENT_DEF name while walking (stack-based via param). */
static void walk_checks(Node *n, const NameSet *defs, CheckCtx *ctx,
                        const char *enclosing_comp, DiagList *out,
                        const char *file) {
  if (!n) return;

  const char *comp = enclosing_comp;
  if (n->type == NODE_COMPONENT_DEF && n->value)
    comp = n->value;

  /* Empty use path (USE nodes normally removed after project resolve) */
  if (n->type == NODE_USE) {
    if (!n->value || !n->value[0]) {
      diag_emit(out, DIAG_ERROR, file, n->line, n->col, "empty use path");
    }
  }

  /* Unknown PascalCase component usage */
  if (n->type == NODE_ELEMENT && n->value && is_pascal_case(n->value)) {
    if (!name_set_has(defs, n->value)) {
      diag_emit(out, DIAG_ERROR, file, n->line, n->col,
                "unknown component '%s'", n->value);
    }
  }

  /* Attr traps + unknown attrs on built-in tags */
  if (n->type == NODE_ELEMENT && n->value) {
    int builtin = cord_is_builtin_tag(n->value);
    int has_alt = 0;
    for (size_t i = 0; i < n->children_len; i++) {
      Node *ch = n->children[i];
      if (!ch) continue;
      if (ch->type == NODE_ATTR && ch->value) {
        if (strcmp(ch->value, "alt") == 0) has_alt = 1;
        if (cord_is_forbidden_jsx_attr(ch->value)) {
          diag_emit(out, DIAG_ERROR, file, ch->line, ch->col,
                    "JSX attribute '%s' is not Cordlang — use @events / style "
                    "attrs (see docs/schema/attrs.json)",
                    ch->value);
        } else if (builtin && !cord_is_known_attr(ch->value)) {
          diag_emit(out, DIAG_WARN, file, ch->line, ch->col,
                    "unknown attribute '%s' on tag '%s'", ch->value, n->value);
        }
      }
      if (ch->type == NODE_BOOL_ATTR && ch->value) {
        if (cord_is_forbidden_jsx_attr(ch->value)) {
          diag_emit(out, DIAG_ERROR, file, ch->line, ch->col,
                    "JSX attribute '%s' is not Cordlang", ch->value);
        } else if (builtin && !cord_is_known_attr(ch->value)) {
          diag_emit(out, DIAG_WARN, file, ch->line, ch->col,
                    "unknown attribute '%s' on tag '%s'", ch->value, n->value);
        }
      }
    }
    /* a11y: img without alt */
    if (builtin && strcmp(n->value, "img") == 0 && !has_alt) {
      diag_emit(out, DIAG_WARN, file, n->line, n->col,
                "img without alt — add alt=\"...\" (or alt=\"\" if decorative)");
    }
  }

  /* provide without matching context declaration */
  if (n->type == NODE_ELEMENT && n->value && strcmp(n->value, "provide") == 0) {
    const char *ctx_name = NULL;
    for (size_t i = 0; i < n->children_len; i++) {
      Node *ch = n->children[i];
      if (ch && ch->type == NODE_ATTR && ch->value &&
          strcmp(ch->value, "context") == 0 && ch->value2) {
        ctx_name = ch->value2;
        break;
      }
    }
    if (ctx_name && !ctx_has_context(ctx, ctx_name)) {
      diag_emit(out, DIAG_WARN, file, n->line, n->col,
                "provide '%s' has no matching context declaration", ctx_name);
    }
  }

  /* params without matching route :param */
  if (n->type == NODE_PARAMS_DECL && n->value && n->value[0] && comp) {
    char param_names[16][NAME_LEN];
    int n_params = 0;
    const char *p = n->value;
    while (*p && n_params < 16) {
      while (*p && (isspace((unsigned char)*p) || *p == ',')) p++;
      if (!*p) break;
      size_t i = 0;
      while (*p && *p != ',' && !isspace((unsigned char)*p) &&
             i + 1 < NAME_LEN) {
        param_names[n_params][i++] = *p++;
      }
      param_names[n_params][i] = '\0';
      if (i > 0) n_params++;
    }

    for (int pi = 0; pi < n_params; pi++) {
      int found = 0;
      for (int ri = 0; ri < ctx->n_routes && !found; ri++) {
        if (strcmp(ctx->routes[ri].target, comp) != 0) continue;
        char rparams[16][NAME_LEN];
        int nr = extract_route_params(ctx->routes[ri].path, rparams, 16);
        for (int j = 0; j < nr; j++) {
          if (strcmp(rparams[j], param_names[pi]) == 0) {
            found = 1;
            break;
          }
        }
      }
      /* Only warn when this component is a route target somewhere */
      int is_route_target = 0;
      for (int ri = 0; ri < ctx->n_routes; ri++) {
        if (strcmp(ctx->routes[ri].target, comp) == 0) {
          is_route_target = 1;
          break;
        }
      }
      if (is_route_target && !found) {
        diag_emit(out, DIAG_WARN, file, n->line, n->col,
                  "params '%s' has no matching :%s in routes to '%s'",
                  param_names[pi], param_names[pi], comp);
      }
    }
  }

  for (size_t i = 0; i < n->children_len; i++)
    walk_checks(n->children[i], defs, ctx, comp, out, file);
}

static void check_routes(const CheckCtx *ctx, const NameSet *defs, DiagList *out,
                         const char *file) {
  for (int i = 0; i < ctx->n_routes; i++) {
    const RouteInfo *r = &ctx->routes[i];
    if (!r->target[0]) {
      diag_emit(out, DIAG_ERROR, file, r->line, r->col,
                "route target missing for path '%s'",
                r->path[0] ? r->path : "?");
      continue;
    }
    if (!name_set_has(defs, r->target)) {
      diag_emit(out, DIAG_ERROR, file, r->line, r->col,
                "route target '%s' is not a known component", r->target);
    }
  }
}

int check_service_run(const char *entry_path, DiagList *out) {
  if (!out) return 1;
  /* out may already be initialized by caller; do not wipe existing diags */

  if (!entry_path || !*entry_path) {
    diag_emit(out, DIAG_ERROR, "<check>", 0, 0, "no entry path");
    return 1;
  }

  CompileResult result = compiler_parse_project(entry_path);
  if (!result.ok || !result.ast || !result.ast->root) {
    diag_emit(out, DIAG_ERROR, entry_path, 0, 0, "%s",
              result.error ? result.error : "parse failed");
    compiler_result_free(&result);
    return 1;
  }

  NameSet defs = {0};
  NameSet dups = {0};
  CheckCtx ctx = {0};

  collect_defs(result.ast->root, &defs, &dups, out, entry_path);
  collect_contexts_and_routes(result.ast->root, &ctx);
  check_routes(&ctx, &defs, out, entry_path);
  check_prop_types(result.ast->root, out, entry_path);
  walk_checks(result.ast->root, &defs, &ctx, NULL, out, entry_path);

  compiler_result_free(&result);
  return diag_error_count(out) > 0 ? 1 : 0;
}
