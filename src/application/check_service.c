#include "application/check_service.h"

#include "application/ports/compiler_port.h"
#include "adapters/outbound/backends/preset_registry.h"
#include "application/ports/fs_port.h"
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
  char presets[16][PRESET_NAME_LEN];
  int n_presets;
} CheckCtx;

static void load_presets_for_entry(const char *entry_path, CheckCtx *ctx) {
  if (!entry_path || !ctx) return;
  char dir[512];
  strncpy(dir, entry_path, sizeof(dir) - 1);
  dir[sizeof(dir) - 1] = '\0';
  /* If entry is a file, strip basename */
  char *slash = strrchr(dir, '/');
#ifdef _WIN32
  char *bslash = strrchr(dir, '\\');
  if (bslash && (!slash || bslash > slash)) slash = bslash;
#endif
  if (slash && strstr(slash, ".cord")) *slash = '\0';
  if (!dir[0]) strcpy(dir, ".");
  /* Walk up looking for cordlang.json */
  for (int up = 0; up < 6; up++) {
    char *cfg = fs_join(dir, "cordlang.json");
    if (cfg && fs_exists(cfg)) {
      free(cfg);
      ctx->n_presets = preset_load_from_project(dir, ctx->presets, 16);
      return;
    }
    free(cfg);
    char *parent = fs_join(dir, "..");
    if (!parent) break;
    strncpy(dir, parent, sizeof(dir) - 1);
    free(parent);
  }
}

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

  if (n->type == NODE_FOREIGN && n->value && n->value[0]) {
    if (!name_set_has(defs, n->value))
      name_set_add(defs, n->value, n->line, n->col);
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

  /* Unknown PascalCase component usage (skip known JSX hook traps) */
  if (n->type == NODE_ELEMENT && n->value && is_pascal_case(n->value)) {
    if (cord_is_jsx_hook_name(n->value)) {
      diag_emit_ex(out, DIAG_ERROR, file, n->line, n->col, "jsx-hook",
                   "use state / effect / link — not React hooks or <Link>",
                   "JSX/React name '%s' is not Cordlang", n->value);
    } else if (!name_set_has(defs, n->value)) {
      diag_emit(out, DIAG_ERROR, file, n->line, n->col,
                "unknown component '%s'", n->value);
    }
  }

  /* CamelCase hooks as tags: useState, useEffect, … */
  if (n->type == NODE_ELEMENT && n->value && cord_is_jsx_hook_name(n->value) &&
      !is_pascal_case(n->value)) {
    diag_emit_ex(out, DIAG_ERROR, file, n->line, n->col, "jsx-hook",
                 "use state / effect — not React hooks in .cord",
                 "JSX/React name '%s' is not Cordlang", n->value);
  }

  /* foreign without any module binding */
  if (n->type == NODE_FOREIGN && n->value) {
    int has_mod = (n->value2 && n->value2[0]);
    if (!has_mod) {
      for (size_t i = 0; i < n->children_len; i++) {
        Node *ch = n->children[i];
        if (ch && ch->type == NODE_ATTR && ch->value2 && ch->value2[0])
          has_mod = 1;
      }
    }
    if (!has_mod) {
      diag_emit_ex(out, DIAG_ERROR, file, n->line, n->col, "foreign-unbound",
                   "add: react from \"pkg\" (and other backends as needed)",
                   "foreign '%s' has no module binding", n->value);
    }
  }

  /* Capability tags require presets */
  if (n->type == NODE_ELEMENT && n->value) {
    const char *need = NULL;
    if (strcmp(n->value, "icon") == 0) need = PRESET_ICONS;
    else if (strcmp(n->value, "motion") == 0 || strcmp(n->value, "Motion") == 0)
      need = PRESET_MOTION;
    else if (strcmp(n->value, "chart") == 0 || strcmp(n->value, "Chart") == 0)
      need = PRESET_CHARTS;
    if (need && !preset_list_has(ctx->presets, ctx->n_presets, need)) {
      diag_emit_ex(out, DIAG_ERROR, file, n->line, n->col, "missing-preset",
                   "cordlang preset add … then re-run",
                   "tag '%s' needs capability preset '%s'", n->value, need);
    }
  }

  /* foreign without binding for any backend still OK; unbound checked at emit */

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
          diag_emit_ex(out, DIAG_ERROR, file, ch->line, ch->col, "jsx-attr",
                       cord_jsx_attr_hint(ch->value),
                       "JSX attribute '%s' is not Cordlang — use @events / "
                       "style attrs (see docs/schema/attrs.json)",
                       ch->value);
        } else if (strcmp(ch->value, "purpose") == 0 && ch->value2 &&
                   !cord_is_purpose_vocab(ch->value2)) {
          diag_emit_ex(out, DIAG_WARN, file, ch->line, ch->col, "semantic-vocab",
                       "use navigation|content|action|form|status|decoration|"
                       "landmark",
                       "purpose='%s' is outside documented vocabulary",
                       ch->value2);
        } else if (strcmp(ch->value, "importance") == 0 && ch->value2 &&
                   !cord_is_importance_vocab(ch->value2)) {
          diag_emit_ex(out, DIAG_WARN, file, ch->line, ch->col, "semantic-vocab",
                       "use primary|secondary|tertiary|optional|critical",
                       "importance='%s' is outside documented vocabulary",
                       ch->value2);
        } else if (builtin && !cord_is_known_attr(ch->value)) {
          diag_emit(out, DIAG_WARN, file, ch->line, ch->col,
                    "unknown attribute '%s' on tag '%s'", ch->value, n->value);
        }
      }
      if (ch->type == NODE_BOOL_ATTR && ch->value) {
        if (cord_is_forbidden_jsx_attr(ch->value)) {
          diag_emit_ex(out, DIAG_ERROR, file, ch->line, ch->col, "jsx-attr",
                       cord_jsx_attr_hint(ch->value),
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

  /* AI trap: bare {expr} in text instead of #{expr} */
  if (n->type == NODE_TEXT && n->value) {
    const char *s = n->value;
    for (size_t i = 0; s[i]; i++) {
      if (s[i] != '{') continue;
      if (i > 0 && s[i - 1] == '#') continue;
      /* Skip JSON-ish or CSS; require {ident} shape */
      size_t j = i + 1;
      if (!s[j] || !(isalpha((unsigned char)s[j]) || s[j] == '_')) continue;
      while (s[j] && (isalnum((unsigned char)s[j]) || s[j] == '_' ||
                      s[j] == '.'))
        j++;
      if (s[j] == '}') {
        diag_emit_ex(out, DIAG_ERROR, file, n->line, n->col, "bad-interp",
                     "use #{expr} for interpolation (not {expr})",
                     "JSX-style '{…}' in text — Cordlang uses #{…}");
        break;
      }
    }
  }

  /* AI trap: .map( — use for … key= */
  if (n->value && strstr(n->value, ".map(")) {
    diag_emit_ex(out, DIAG_ERROR, file, n->line, n->col, "jsx-map",
                 "use for item in list key=…",
                 "JSX-style '.map(' in .cord — Cordlang uses for … key=");
  } else if (n->value2 && strstr(n->value2, ".map(")) {
    diag_emit_ex(out, DIAG_ERROR, file, n->line, n->col, "jsx-map",
                 "use for item in list key=…",
                 "JSX-style '.map(' in .cord — Cordlang uses for … key=");
  }

  /* AI trap: angle-bracket JSX tags in text / string values */
  if (n->value) {
    const char *s = n->value;
    for (size_t i = 0; s[i]; i++) {
      if (s[i] != '<') continue;
      char c = s[i + 1];
      if (!c) break;
      if (c == '/' || isalpha((unsigned char)c)) {
        diag_emit_ex(out, DIAG_ERROR, file, n->line, n->col, "jsx-tag",
                     "use Cord tags (div, col, link) — not <Tag>",
                     "JSX-style angle-bracket tag in .cord text");
        break;
      }
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

static int check_service_on_ast(Node *root, const char *file_label,
                                DiagList *out) {
  if (!out || !root) return 1;
  NameSet defs = {0};
  NameSet dups = {0};
  CheckCtx ctx = {0};

  load_presets_for_entry(file_label, &ctx);
  collect_defs(root, &defs, &dups, out, file_label);
  collect_contexts_and_routes(root, &ctx);
  check_routes(&ctx, &defs, out, file_label);
  check_prop_types(root, out, file_label);
  walk_checks(root, &defs, &ctx, NULL, out, file_label);
  return diag_error_count(out) > 0 ? 1 : 0;
}

int check_service_run_source(const char *file_label, const char *source,
                             size_t source_len, DiagList *out) {
  if (!out) return 1;
  const char *label = (file_label && *file_label) ? file_label : "<buffer>";
  if (!source) {
    diag_emit(out, DIAG_ERROR, label, 0, 0, "no source");
    return 1;
  }

  CompileResult result = compiler_parse_source(source, source_len);
  if (!result.ok || !result.ast || !result.ast->root) {
    diag_emit(out, DIAG_ERROR, label, 0, 0, "%s",
              result.error ? result.error : "parse failed");
    compiler_result_free(&result);
    return 1;
  }

  int rc = check_service_on_ast(result.ast->root, label, out);
  compiler_result_free(&result);
  return rc;
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

  int rc = check_service_on_ast(result.ast->root, entry_path, out);
  compiler_result_free(&result);
  return rc;
}
