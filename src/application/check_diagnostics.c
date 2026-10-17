#include "application/check_internal.h"

void walk_checks(Node *n, const NameSet *defs, CheckCtx *ctx,
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
