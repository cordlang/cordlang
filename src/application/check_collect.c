#include "application/check_internal.h"

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

int is_pascal_case(const char *s) {
  return s && s[0] && isupper((unsigned char)s[0]);
}

int name_set_has(const NameSet *s, const char *name) {
  if (!s || !name) return 0;
  for (int i = 0; i < s->count; i++) {
    if (strcmp(s->names[i], name) == 0) return 1;
  }
  return 0;
}

int name_set_add(NameSet *s, const char *name, int line, int col) {
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

int ctx_has_context(const CheckCtx *c, const char *name) {
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

int extract_route_params(const char *path, char names[][NAME_LEN],
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

void collect_defs(Node *n, NameSet *defs, NameSet *dup_report,
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

void collect_contexts_and_routes(Node *n, CheckCtx *ctx) {
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

void check_prop_types(Node *n, DiagList *out, const char *file) {
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
