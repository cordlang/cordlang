#include "adapters/outbound/backends/svelte/svelte_backend.h"
#include "adapters/outbound/backends/ir_walk.h"
#include "adapters/outbound/backends/source_attr.h"
#include "adapters/outbound/backends/theme_css.h"
#include "adapters/outbound/html_escape.h"
#include "domain/interp.h"
#include "domain/ir.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * IR-2 pure Svelte codegen: walks IrNode only for body / module emission.
 * No ir_origin / AST Node * required for generate_from_ir.
 */

/* ── string buffer ─────────────────────────────────────── */

typedef struct {
  char *buf;
  size_t len;
  size_t cap;
} StrBuf;

static void sb_init(StrBuf *sb) {
  sb->cap = 65536;
  sb->len = 0;
  sb->buf = calloc(sb->cap, 1);
}

static void sb_oom(void) {
  fprintf(stderr, "fatal: out of memory (StrBuf)\n");
  exit(1);
}

static void sb_append(StrBuf *sb, const char *s) {
  if (!s) return;
  size_t slen = strlen(s);
  if (sb->len + slen + 1 >= sb->cap) {
    while (sb->len + slen + 1 >= sb->cap) {
      if (sb->cap > (size_t)-1 / 2) sb_oom();
      sb->cap *= 2;
    }
    char *nbuf = realloc(sb->buf, sb->cap);
    if (!nbuf) sb_oom();
    sb->buf = nbuf;
  }
  memcpy(sb->buf + sb->len, s, slen);
  sb->len += slen;
  sb->buf[sb->len] = '\0';
}

static void sb_appendf(StrBuf *sb, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  int n = vsnprintf(NULL, 0, fmt, args);
  va_end(args);
  if (n < 0) return;
  if (sb->len + (size_t)n + 1 >= sb->cap) {
    while (sb->len + (size_t)n + 1 >= sb->cap) {
      if (sb->cap > (size_t)-1 / 2) sb_oom();
      sb->cap *= 2;
    }
    char *nbuf = realloc(sb->buf, sb->cap);
    if (!nbuf) sb_oom();
    sb->buf = nbuf;
  }
  va_start(args, fmt);
  vsnprintf(sb->buf + sb->len, sb->cap - sb->len, fmt, args);
  va_end(args);
  sb->len += (size_t)n;
  sb->buf[sb->len] = '\0';
}

static void sb_indent(StrBuf *sb, int depth) {
  for (int i = 0; i < depth; i++) sb_append(sb, "  ");
}

/* ── helpers ───────────────────────────────────────────── */

static int looks_like_js_expr(const char *s) {
  if (!s || !*s) return 0;
  if (!(isalpha((unsigned char)s[0]) || s[0] == '_' || s[0] == '$')) return 0;
  int after_dot = 0;
  for (const char *p = s; *p; p++) {
    if (*p == '.') {
      if (after_dot) return 0;
      after_dot = 1;
      continue;
    }
    if (!(isalnum((unsigned char)*p) || *p == '_' || *p == '$')) return 0;
    after_dot = 0;
  }
  return !after_dot;
}

/* Mirror React emit_jsx_value: PascalCase / Title Case strings stay quoted. */
static void emit_svelte_prop_value(StrBuf *sb, const char *val) {
  if (!val) {
    sb_append(sb, "{undefined}");
    return;
  }
  if (irw_looks_number(val) || irw_looks_bool(val)) {
    sb_appendf(sb, "{%s}", val);
    return;
  }
  if ((isalpha((unsigned char)val[0]) || val[0] == '_') &&
      strchr(val, ' ') == NULL && strchr(val, '"') == NULL) {
    int has_dot = strchr(val, '.') != NULL;
    int all_ident = 1;
    for (const char *p = val; *p; p++) {
      if (!(isalnum((unsigned char)*p) || *p == '_' || *p == '.' || *p == '$')) {
        all_ident = 0;
        break;
      }
    }
    if (all_ident &&
        (has_dot || islower((unsigned char)val[0]) || val[0] == '_')) {
      sb_appendf(sb, "{%s}", val);
      return;
    }
  }
  sb_appendf(sb, "\"%s\"", val);
}

static int is_style_attr(const char *name) {
  return irw_is_style_attr(name);
}

/* Phase E: store names in current module (for {$store} auto-prefix) */
static char g_sv_stores[32][64];
static int g_sv_n_stores;

static void sv_reset_stores(void) { g_sv_n_stores = 0; }

static void sv_add_store(const char *name) {
  if (!name || !*name || g_sv_n_stores >= 32) return;
  for (int i = 0; i < g_sv_n_stores; i++)
    if (strcmp(g_sv_stores[i], name) == 0) return;
  snprintf(g_sv_stores[g_sv_n_stores], sizeof(g_sv_stores[0]), "%s", name);
  g_sv_n_stores++;
}

static int sv_is_store_expr(const char *expr) {
  if (!expr || !*expr) return 0;
  for (const char *p = expr; *p; p++) {
    if (!(isalnum((unsigned char)*p) || *p == '_' || *p == '$')) return 0;
  }
  for (int i = 0; i < g_sv_n_stores; i++)
    if (strcmp(g_sv_stores[i], expr) == 0) return 1;
  return 0;
}

static int is_svelte_directive_attr(const char *name) {
  return name && (strcmp(name, "use") == 0 || strcmp(name, "transition") == 0 ||
                  strcmp(name, "in") == 0 || strcmp(name, "out") == 0 ||
                  strcmp(name, "animate") == 0);
}

static void emit_svelte_directive(StrBuf *sb, const char *dir, const char *val) {
  if (!dir || !val || !*val) return;
  const char *lp = strchr(val, '(');
  if (lp && lp > val) {
    char name[96];
    size_t nl = (size_t)(lp - val);
    if (nl >= sizeof(name)) nl = sizeof(name) - 1;
    memcpy(name, val, nl);
    name[nl] = '\0';
    const char *rp = strrchr(val, ')');
    if (rp && rp > lp + 1) {
      char args[384];
      size_t al = (size_t)(rp - lp - 1);
      if (al >= sizeof(args)) al = sizeof(args) - 1;
      memcpy(args, lp + 1, al);
      args[al] = '\0';
      char *a = args;
      while (*a == ' ') a++;
      size_t alen = strlen(a);
      while (alen > 0 && a[alen - 1] == ' ') a[--alen] = '\0';
      if (*a)
        sb_appendf(sb, " %s:%s={%s}", dir, name, a);
      else
        sb_appendf(sb, " %s:%s", dir, name);
      return;
    }
  }
  sb_appendf(sb, " %s:%s", dir, val);
}

static int is_svelte_transition_fn(const char *name) {
  return name && (strcmp(name, "fade") == 0 || strcmp(name, "fly") == 0 ||
                  strcmp(name, "slide") == 0 || strcmp(name, "scale") == 0 ||
                  strcmp(name, "blur") == 0 || strcmp(name, "draw") == 0 ||
                  strcmp(name, "crossfade") == 0);
}

static int is_svelte_animate_fn(const char *name) {
  return name && strcmp(name, "flip") == 0;
}

static void directive_fn_name(const char *val, char *out, size_t outsz) {
  if (!val || !out || outsz == 0) {
    if (out && outsz) out[0] = '\0';
    return;
  }
  size_t i = 0;
  while (val[i] && val[i] != '(' && i + 1 < outsz) {
    out[i] = val[i];
    i++;
  }
  out[i] = '\0';
}

typedef struct {
  char transition_fns[16][32];
  int n_transition;
  char animate_fns[8][32];
  int n_animate;
  int need_portal;
  int need_writable;
} SvModuleNeeds;

static void sv_needs_add_trans(SvModuleNeeds *n, const char *fn) {
  if (!n || !fn || !*fn || !is_svelte_transition_fn(fn)) return;
  for (int i = 0; i < n->n_transition; i++)
    if (strcmp(n->transition_fns[i], fn) == 0) return;
  if (n->n_transition < 16) {
    snprintf(n->transition_fns[n->n_transition], sizeof(n->transition_fns[0]),
             "%s", fn);
    n->n_transition++;
  }
}

static void sv_needs_add_anim(SvModuleNeeds *n, const char *fn) {
  if (!n || !fn || !*fn || !is_svelte_animate_fn(fn)) return;
  for (int i = 0; i < n->n_animate; i++)
    if (strcmp(n->animate_fns[i], fn) == 0) return;
  if (n->n_animate < 8) {
    snprintf(n->animate_fns[n->n_animate], sizeof(n->animate_fns[0]), "%s", fn);
    n->n_animate++;
  }
}

static int attr_is_true(const IrNode *a) {
  return a && a->value &&
         (a->value[0] == '\0' || strcmp(a->value, "true") == 0);
}

/* Full class collection matching legacy AST fidelity (not only irw). */
static void collect_classes_ir(char *classes, size_t sz, const IrNode *node,
                               const char *base) {
  classes[0] = '\0';
  if (base) strncat(classes, base, sz - 1);
  if (!node) return;

  int has_between = 0;
  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *c = node->kids[i];
    if (!c || c->kind != IR_ATTR || !c->name) continue;
    if (strcmp(c->name, "between") == 0) has_between = 1;
  }

  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *c = node->kids[i];
    if (!c || c->kind != IR_ATTR || !c->name) continue;
    if (c->name[0] == '_' && c->name[1] == '_') continue;
    char vbuf[96];
    const char *k = c->name;
    const char *v = c->value ? c->value : "";

    /* Bool-like attrs (name is the flag / size token) */
    if (attr_is_true(c) || (!c->value || !c->value[0])) {
      if (strcmp(k, "between") == 0)
        strncat(classes,
                strstr(classes, "flex") ? " justify-between"
                                        : " flex justify-between",
                sz - strlen(classes) - 1);
      else if (strcmp(k, "center") == 0)
        strncat(classes,
                has_between ? " flex items-center"
                            : " flex items-center justify-center",
                sz - strlen(classes) - 1);
      else if (strcmp(k, "bold") == 0)
        strncat(classes, " font-bold", sz - strlen(classes) - 1);
      else if (strcmp(k, "muted") == 0)
        strncat(classes, " text-muted", sz - strlen(classes) - 1);
      else if (strcmp(k, "font-mono") == 0)
        strncat(classes, " font-mono", sz - strlen(classes) - 1);
      else if (strcmp(k, "flex-1") == 0)
        strncat(classes, " flex-1", sz - strlen(classes) - 1);
      else if (strcmp(k, "border") == 0)
        strncat(classes, " border border-gray-200", sz - strlen(classes) - 1);
      else if (strcmp(k, "sticky") == 0)
        strncat(classes, " sticky top-0", sz - strlen(classes) - 1);
      else if (strcmp(k, "primary") == 0)
        strncat(classes, " btn-primary", sz - strlen(classes) - 1);
      else if (strcmp(k, "outline") == 0)
        strncat(classes, " btn-outline", sz - strlen(classes) - 1);
      else if (strcmp(k, "ghost") == 0)
        strncat(classes, " btn-ghost", sz - strlen(classes) - 1);
      else if (strcmp(k, "xl") == 0)
        strncat(classes, " text-xl", sz - strlen(classes) - 1);
      else if (strcmp(k, "2xl") == 0)
        strncat(classes, " text-2xl", sz - strlen(classes) - 1);
      else if (strcmp(k, "4xl") == 0)
        strncat(classes, " text-4xl", sz - strlen(classes) - 1);
      else if (strcmp(k, "lg") == 0)
        strncat(classes, " text-lg", sz - strlen(classes) - 1);
      if (attr_is_true(c) || !c->value || !c->value[0]) {
        /* valued style keys with "true" already handled above for flags */
        if (strcmp(k, "variant") && strcmp(k, "size") && strcmp(k, "color") &&
            strcmp(k, "gap") && strcmp(k, "cols") && strcmp(k, "p") &&
            strcmp(k, "bg") && strcmp(k, "shadow") && strcmp(k, "rounded") &&
            strcmp(k, "max-w") && strcmp(k, "w") && strcmp(k, "h") &&
            strcmp(k, "min-h") && strcmp(k, "mx") && strcmp(k, "my") &&
            strcmp(k, "px") && strcmp(k, "py") && strcmp(k, "m") &&
            strcmp(k, "border"))
          continue;
      }
    }

    if (!c->value) continue;
    if (strcmp(k, "variant") == 0) {
      snprintf(vbuf, sizeof(vbuf), " btn-%s", v);
      strncat(classes, vbuf, sz - strlen(classes) - 1);
    } else if (strcmp(k, "size") == 0) {
      snprintf(vbuf, sizeof(vbuf), " text-%s", v);
      strncat(classes, vbuf, sz - strlen(classes) - 1);
    } else if (strcmp(k, "color") == 0) {
      if (theme_is_color_token(v)) {
        char tok[64];
        if (theme_token_name(v, tok, sizeof(tok))) {
          snprintf(vbuf, sizeof(vbuf), " text-[var(--color-%s)]", tok);
          strncat(classes, vbuf, sz - strlen(classes) - 1);
        }
      } else {
        snprintf(vbuf, sizeof(vbuf), " text-%s", v);
        strncat(classes, vbuf, sz - strlen(classes) - 1);
      }
    } else if (strcmp(k, "gap") == 0) {
      snprintf(vbuf, sizeof(vbuf), " gap-%s", v);
      strncat(classes, vbuf, sz - strlen(classes) - 1);
    } else if (strcmp(k, "cols") == 0) {
      snprintf(vbuf, sizeof(vbuf), " grid-cols-%s", v);
      strncat(classes, vbuf, sz - strlen(classes) - 1);
    } else if (strcmp(k, "p") == 0) {
      snprintf(vbuf, sizeof(vbuf), " p-%s", v);
      strncat(classes, vbuf, sz - strlen(classes) - 1);
    } else if (strcmp(k, "px") == 0) {
      snprintf(vbuf, sizeof(vbuf), " px-%s", v);
      strncat(classes, vbuf, sz - strlen(classes) - 1);
    } else if (strcmp(k, "py") == 0) {
      snprintf(vbuf, sizeof(vbuf), " py-%s", v);
      strncat(classes, vbuf, sz - strlen(classes) - 1);
    } else if (strcmp(k, "m") == 0) {
      snprintf(vbuf, sizeof(vbuf), " m-%s", v);
      strncat(classes, vbuf, sz - strlen(classes) - 1);
    } else if (strcmp(k, "mx") == 0) {
      snprintf(vbuf, sizeof(vbuf), " mx-%s", v);
      strncat(classes, vbuf, sz - strlen(classes) - 1);
    } else if (strcmp(k, "my") == 0) {
      snprintf(vbuf, sizeof(vbuf), " my-%s", v);
      strncat(classes, vbuf, sz - strlen(classes) - 1);
    } else if (strcmp(k, "w") == 0) {
      snprintf(vbuf, sizeof(vbuf), " w-%s", v);
      strncat(classes, vbuf, sz - strlen(classes) - 1);
    } else if (strcmp(k, "h") == 0) {
      snprintf(vbuf, sizeof(vbuf), " h-%s", v);
      strncat(classes, vbuf, sz - strlen(classes) - 1);
    } else if (strcmp(k, "min-h") == 0) {
      snprintf(vbuf, sizeof(vbuf), " min-h-%s", v);
      strncat(classes, vbuf, sz - strlen(classes) - 1);
    } else if (strcmp(k, "border") == 0 && !attr_is_true(c)) {
      snprintf(vbuf, sizeof(vbuf), " border border-%s", v);
      strncat(classes, vbuf, sz - strlen(classes) - 1);
    } else if (strcmp(k, "bg") == 0) {
      if (theme_is_color_token(v)) {
        char tok[64];
        if (theme_token_name(v, tok, sizeof(tok))) {
          snprintf(vbuf, sizeof(vbuf), " bg-[var(--color-%s)]", tok);
          strncat(classes, vbuf, sz - strlen(classes) - 1);
        }
      } else {
        snprintf(vbuf, sizeof(vbuf), " bg-%s", v);
        strncat(classes, vbuf, sz - strlen(classes) - 1);
      }
    } else if (strcmp(k, "shadow") == 0) {
      snprintf(vbuf, sizeof(vbuf), " shadow-%s", v);
      strncat(classes, vbuf, sz - strlen(classes) - 1);
    } else if (strcmp(k, "rounded") == 0) {
      snprintf(vbuf, sizeof(vbuf), " rounded-%s", v);
      strncat(classes, vbuf, sz - strlen(classes) - 1);
    } else if (strcmp(k, "max-w") == 0) {
      snprintf(vbuf, sizeof(vbuf), " max-w-%s", v);
      strncat(classes, vbuf, sz - strlen(classes) - 1);
    }
  }
}

/* ── project partition (IR) ────────────────────────────── */

typedef enum { SK_PAGE = 0, SK_COMPONENT = 1, SK_LAYOUT = 2 } SvelteKind;

#define SV_MAX_UNITS 64
#define SV_MAX_USED 24
#define SV_MAX_ROUTES 64

typedef struct {
  char name[96];
  char dir[16];
  char rel[128];
  char source_path[256];
  IrNode *ir_def; /* IR_COMPONENT / IR_LAYOUT */
  Node *def;      /* optional legacy; unused by pure IR path */
  SvelteKind kind;
  int use_link;
  int use_slot;
  int use_context;
  int use_params;
  char used[SV_MAX_USED][96];
  int n_used;
} SvelteUnit;

typedef struct {
  SvelteUnit units[SV_MAX_UNITS];
  int n_units;
  IrNode *routes[SV_MAX_ROUTES];
  int n_routes;
  int has_router;
  IrNode *contexts[32]; /* IR_HOOK context */
  int n_contexts;
  char lazy_route_names[16][64];
  int n_lazy_routes;
  int truncated;
} SvelteProject;

typedef struct {
  const char *ctx_name;
  const char *val;
} ProvideInfo;

static int name_ends_with(const char *s, const char *suffix) {
  size_t ls = s ? strlen(s) : 0, lf = strlen(suffix);
  return ls >= lf && strcmp(s + ls - lf, suffix) == 0;
}

static int is_route_target_ir(const char *name, IrNode **routes, int n) {
  if (!name) return 0;
  for (int i = 0; i < n; i++)
    if (routes[i] && routes[i]->value && strcmp(routes[i]->value, name) == 0)
      return 1;
  return 0;
}

static void unit_add_used(SvelteUnit *u, const char *name) {
  if (!name || !irw_is_pascal(name)) return;
  if (strcmp(name, u->name) == 0) return;
  for (int i = 0; i < u->n_used; i++)
    if (strcmp(u->used[i], name) == 0) return;
  if (u->n_used < SV_MAX_USED) {
    snprintf(u->used[u->n_used], sizeof(u->used[0]), "%s", name);
    u->n_used++;
  }
}

static int route_has_lazy_attr_ir(IrNode *route) {
  if (!route) return 0;
  for (size_t i = 0; i < route->n_kids; i++) {
    IrNode *c = route->kids[i];
    if (c && c->kind == IR_ATTR && c->name && strcmp(c->name, "lazy") == 0 &&
        attr_is_true(c))
      return 1;
  }
  return 0;
}

static int project_is_lazy_route_name(SvelteProject *proj, const char *name) {
  if (!proj || !name) return 0;
  for (int i = 0; i < proj->n_lazy_routes; i++) {
    if (strcmp(proj->lazy_route_names[i], name) == 0) return 1;
  }
  return 0;
}

static int route_is_lazy_ir(SvelteProject *proj, IrNode *route) {
  if (!route) return 0;
  if (route_has_lazy_attr_ir(route)) return 1;
  if (route->value && project_is_lazy_route_name(proj, route->value)) return 1;
  return 0;
}

static void scan_deps_ir(IrNode *n, SvelteUnit *u) {
  if (!n || !u) return;
  if (n->kind == IR_SLOT) u->use_slot = 1;
  if (irw_hook_is(n, "ctx") || irw_hook_is(n, "context")) u->use_context = 1;
  if (irw_hook_is(n, "params")) u->use_params = 1;
  if (n->kind == IR_ELEMENT && n->name) {
    if (irw_is_pascal(n->name)) unit_add_used(u, n->name);
    if (strcmp(n->name, "link") == 0) u->use_link = 1;
    if (strcmp(n->name, "provide") == 0) u->use_context = 1;
  }
  for (size_t i = 0; i < n->n_kids; i++) scan_deps_ir(n->kids[i], u);
}

static void scan_module_needs_ir(IrNode *n, SvModuleNeeds *needs) {
  if (!n || !needs) return;
  if (irw_hook_is(n, "portal")) needs->need_portal = 1;
  if (n->kind == IR_STORE) needs->need_writable = 1;
  if (n->kind == IR_ATTR && n->name && n->value &&
      is_svelte_directive_attr(n->name)) {
    char fn[64];
    directive_fn_name(n->value, fn, sizeof(fn));
    if (strcmp(n->name, "animate") == 0)
      sv_needs_add_anim(needs, fn);
    else if (strcmp(n->name, "transition") == 0 || strcmp(n->name, "in") == 0 ||
             strcmp(n->name, "out") == 0)
      sv_needs_add_trans(needs, fn);
  }
  for (size_t i = 0; i < n->n_kids; i++)
    scan_module_needs_ir(n->kids[i], needs);
}

static void project_add_context_ir(SvelteProject *p, IrNode *c) {
  if (!p || !c || !c->value || p->n_contexts >= 32) return;
  for (int k = 0; k < p->n_contexts; k++) {
    if (p->contexts[k]->value && strcmp(p->contexts[k]->value, c->value) == 0)
      return;
  }
  p->contexts[p->n_contexts++] = c;
}

static void harvest_contexts_ir(SvelteProject *p, IrNode *root) {
  if (!p) return;
  if (root) {
    for (size_t i = 0; i < root->n_kids; i++) {
      IrNode *c = root->kids[i];
      if (irw_hook_is(c, "context")) project_add_context_ir(p, c);
    }
  }
  for (int i = 0; i < p->n_units; i++) {
    IrNode *def = p->units[i].ir_def;
    if (!def) continue;
    for (size_t j = 0; j < def->n_kids; j++) {
      IrNode *c = def->kids[j];
      if (irw_hook_is(c, "context")) project_add_context_ir(p, c);
    }
  }
}

static void collect_provides_ir(IrNode *n, ProvideInfo *out, int *count,
                                int max) {
  if (!n || *count >= max) return;
  if (n->kind == IR_ELEMENT && n->name && strcmp(n->name, "provide") == 0) {
    const char *ctx_name = "Context";
    const char *val = NULL;
    for (size_t i = 0; i < n->n_kids; i++) {
      IrNode *ch = n->kids[i];
      if (ch->kind == IR_ATTR && ch->name) {
        if (strcmp(ch->name, "context") == 0 && ch->value) ctx_name = ch->value;
        if (strcmp(ch->name, "value") == 0) val = ch->value;
      }
    }
    out[*count].ctx_name = ctx_name;
    out[*count].val = val;
    (*count)++;
  }
  for (size_t i = 0; i < n->n_kids; i++)
    collect_provides_ir(n->kids[i], out, count, max);
}

static SvelteUnit *find_unit(SvelteProject *p, const char *name) {
  if (!name) return NULL;
  for (int i = 0; i < p->n_units; i++)
    if (strcmp(p->units[i].name, name) == 0) return &p->units[i];
  return NULL;
}

static void layout_name(const char *orig, char *out, size_t n) {
  if (!orig || !*orig || strcmp(orig, "default") == 0) {
    snprintf(out, n, "DefaultLayout");
    return;
  }
  if (irw_is_pascal(orig)) {
    snprintf(out, n, "%s", orig);
    return;
  }
  snprintf(out, n, "%c%s", (char)toupper((unsigned char)orig[0]), orig + 1);
}

static void project_partition_from_ir(SvelteProject *p, IrNode *root) {
  memset(p, 0, sizeof(*p));
  if (!root) return;

  IrNode *raw_comps[SV_MAX_UNITS];
  IrNode *raw_layouts[16];
  int n_comp = 0, n_layout = 0;

  for (size_t i = 0; i < root->n_kids; i++) {
    IrNode *c = root->kids[i];
    if (!c) continue;
    if (c->kind == IR_COMPONENT) {
      if (n_comp < SV_MAX_UNITS) raw_comps[n_comp++] = c;
      else p->truncated = 1;
    } else if (c->kind == IR_LAYOUT) {
      if (n_layout < 16) raw_layouts[n_layout++] = c;
      else p->truncated = 1;
    } else if (c->kind == IR_ROUTE) {
      if (p->n_routes < SV_MAX_ROUTES) p->routes[p->n_routes++] = c;
      else p->truncated = 1;
    } else if (irw_hook_is(c, "lazy") && c->value) {
      if (p->n_lazy_routes < 16) {
        snprintf(p->lazy_route_names[p->n_lazy_routes],
                 sizeof(p->lazy_route_names[0]), "%s", c->value);
        p->n_lazy_routes++;
      } else {
        p->truncated = 1;
      }
    } else if (irw_hook_is(c, "context")) {
      project_add_context_ir(p, c);
    }
  }

  for (int r = 0; r < p->n_routes; r++) {
    if (!route_has_lazy_attr_ir(p->routes[r]) || !p->routes[r]->value) continue;
    if (project_is_lazy_route_name(p, p->routes[r]->value)) continue;
    if (p->n_lazy_routes < 16) {
      snprintf(p->lazy_route_names[p->n_lazy_routes],
               sizeof(p->lazy_route_names[0]), "%s", p->routes[r]->value);
      p->n_lazy_routes++;
    } else {
      p->truncated = 1;
    }
  }
  p->has_router = (p->n_routes > 0 || n_layout > 0);

  for (int i = 0; i < n_comp; i++) {
    if (p->n_units >= SV_MAX_UNITS) {
      p->truncated = 1;
      break;
    }
    SvelteUnit *u = &p->units[p->n_units++];
    memset(u, 0, sizeof(*u));
    snprintf(u->name, sizeof(u->name), "%s",
             raw_comps[i]->name ? raw_comps[i]->name : "Component");
    u->ir_def = raw_comps[i];
    if (is_route_target_ir(u->name, p->routes, p->n_routes) ||
        name_ends_with(u->name, "Page")) {
      u->kind = SK_PAGE;
      snprintf(u->dir, sizeof(u->dir), "pages");
    } else {
      u->kind = SK_COMPONENT;
      snprintf(u->dir, sizeof(u->dir), "components");
    }
    {
      char relbuf[128];
      snprintf(relbuf, sizeof(relbuf), "%s/%s.svelte", u->dir, u->name);
      snprintf(u->rel, sizeof(u->rel), "%s", relbuf);
    }
    cord_guess_source_path(u->name, u->dir, u->source_path,
                           sizeof(u->source_path));
    scan_deps_ir(u->ir_def, u);
  }

  for (int i = 0; i < n_layout; i++) {
    if (p->n_units >= SV_MAX_UNITS) {
      p->truncated = 1;
      break;
    }
    SvelteUnit *u = &p->units[p->n_units++];
    memset(u, 0, sizeof(*u));
    layout_name(raw_layouts[i]->name, u->name, sizeof(u->name));
    u->ir_def = raw_layouts[i];
    u->kind = SK_LAYOUT;
    snprintf(u->dir, sizeof(u->dir), "layouts");
    {
      char relbuf[128];
      snprintf(relbuf, sizeof(relbuf), "layouts/%s.svelte", u->name);
      snprintf(u->rel, sizeof(u->rel), "%s", relbuf);
    }
    cord_guess_source_path(u->name, u->dir, u->source_path,
                           sizeof(u->source_path));
    u->use_slot = 1;
    p->has_router = 1;
    scan_deps_ir(u->ir_def, u);
  }

  if (p->truncated) {
    fprintf(stderr,
            "error: project exceeds Svelte backend limits "
            "(max %d units, %d routes) — split the app or raise limits\n",
            SV_MAX_UNITS, SV_MAX_ROUTES);
  }
}

/* ── markup generation (pure IR) ───────────────────────── */

static void gen_node_ir(StrBuf *sb, IrNode *node, int depth, int is_layout);
static void gen_children_ir(StrBuf *sb, IrNode *node, int depth, int is_layout);

static void emit_interp_expr(StrBuf *sb, const char *expr) {
  if (!expr) {
    sb_append(sb, "{}");
    return;
  }
  if (sv_is_store_expr(expr))
    sb_appendf(sb, "{$%s}", expr);
  else
    sb_appendf(sb, "{%s}", expr);
}

static void gen_text_or_interp_ir(StrBuf *sb, IrNode *node, int depth) {
  if (node->kind == IR_INTERP && node->value && node->n_kids == 0) {
    sb_indent(sb, depth);
    emit_interp_expr(sb, node->value);
    sb_append(sb, "\n");
    return;
  }
  if (node->kind == IR_INTERP && node->n_kids > 0) {
    sb_indent(sb, depth);
    for (size_t i = 0; i < node->n_kids; i++) {
      IrNode *c = node->kids[i];
      if (c->kind == IR_TEXT && c->value) {
        char *plain = interp_plain_text(c->value);
        if (plain) sb_append(sb, plain);
        free(plain);
      } else if (c->kind == IR_INTERP && c->value)
        emit_interp_expr(sb, c->value);
    }
    sb_append(sb, "\n");
    return;
  }
  if (node->kind == IR_TEXT) {
    if (node->value && strcmp(node->value, "__else__") == 0) return;
    sb_indent(sb, depth);
    if (node->value && interp_has(node->value)) {
      const char *p = node->value;
      while (*p) {
        const char *hash = strstr(p, "#{");
        if (!hash) {
          char *plain = interp_plain_text(p);
          if (plain) sb_append(sb, plain);
          free(plain);
          break;
        }
        if (hash > node->value && hash[-1] == '\\') {
          /* literal \#{…}: emit text before \, then #{…} without '\' */
          if (hash - 1 > p) {
            size_t n = (size_t)((hash - 1) - p);
            char *tmp = malloc(n + 1);
            if (tmp) {
              memcpy(tmp, p, n);
              tmp[n] = '\0';
              sb_append(sb, tmp);
              free(tmp);
            }
          }
          const char *end = strchr(hash + 2, '}');
          if (!end) {
            sb_append(sb, hash);
            break;
          }
          size_t n = (size_t)(end - hash + 1);
          char *tmp = malloc(n + 1);
          if (tmp) {
            memcpy(tmp, hash, n);
            tmp[n] = '\0';
            sb_append(sb, tmp);
            free(tmp);
          }
          p = end + 1;
          continue;
        }
        if (hash > p) {
          char tmp[512];
          size_t n = (size_t)(hash - p);
          if (n >= sizeof(tmp)) n = sizeof(tmp) - 1;
          memcpy(tmp, p, n);
          tmp[n] = '\0';
          sb_append(sb, tmp);
        }
        const char *end = strchr(hash + 2, '}');
        if (!end) {
          sb_append(sb, hash);
          break;
        }
        char expr[256];
        size_t en = (size_t)(end - (hash + 2));
        if (en >= sizeof(expr)) en = sizeof(expr) - 1;
        memcpy(expr, hash + 2, en);
        expr[en] = '\0';
        emit_interp_expr(sb, expr);
        p = end + 1;
      }
      sb_append(sb, "\n");
    } else {
      char *plain = interp_plain_text(node->value);
      sb_append(sb, plain ? plain : "");
      free(plain);
      sb_append(sb, "\n");
    }
  }
}

static int is_markup_child_kind(IrKind k) {
  return k == IR_ELEMENT || k == IR_TEXT || k == IR_FOR || k == IR_IF ||
         k == IR_INTERP || k == IR_SLOT || k == IR_AWAIT || k == IR_SNIPPET ||
         k == IR_RENDER;
}

static void gen_element_ir(StrBuf *sb, IrNode *node, int depth, int is_layout) {
  const char *tag = node->name ? node->name : "div";

  if (strcmp(tag, "slot") == 0 || node->kind == IR_SLOT) {
    sb_indent(sb, depth);
    sb_append(sb, "{@render children?.()}\n");
    return;
  }

  if (strcmp(tag, "provide") == 0) {
    gen_children_ir(sb, node, depth, is_layout);
    return;
  }

  if (irw_is_pascal(tag)) {
    sb_indent(sb, depth);
    sb_appendf(sb, "<%s", tag);
    int has_kids = 0;
    for (size_t i = 0; i < node->n_kids; i++) {
      IrNode *c = node->kids[i];
      if (c->kind == IR_ATTR && c->name && !is_style_attr(c->name) &&
          !(c->name[0] == '_' && c->name[1] == '_')) {
        sb_appendf(sb, " %s=", c->name);
        emit_svelte_prop_value(sb, c->value);
      } else if (c->kind == IR_EVENT && c->name && c->value) {
        int needs_arrow = strchr(c->value, '(') || strchr(c->value, '+') ||
                          strchr(c->value, '-') || strchr(c->value, ' ');
        if (needs_arrow)
          sb_appendf(sb, " on%s={() => %s}", c->name, c->value);
        else
          sb_appendf(sb, " on%s={%s}", c->name, c->value);
      } else if (is_markup_child_kind(c->kind))
        has_kids = 1;
    }
    if (!has_kids) {
      sb_append(sb, " />\n");
      return;
    }
    sb_append(sb, ">\n");
    gen_children_ir(sb, node, depth + 1, is_layout);
    sb_indent(sb, depth);
    sb_appendf(sb, "</%s>\n", tag);
    return;
  }

  const char *html = irw_html_tag(tag);
  if (!html) {
    gen_children_ir(sb, node, depth, is_layout);
    return;
  }

  int self_close = (strcmp(html, "img") == 0 || strcmp(html, "input") == 0);
  sb_indent(sb, depth);
  sb_appendf(sb, "<%s", html);

  char classes[2048];
  collect_classes_ir(classes, sizeof(classes), node, irw_base_class(tag));
  {
    char *cls = classes;
    while (*cls == ' ') cls++;
    if (*cls) {
      char *esc = js_escape_dq_dup(cls);
      sb_appendf(sb, " class=\"%s\"", esc ? esc : "");
      free(esc);
    }
  }

  if (strcmp(tag, "link") == 0) {
    const char *to = "/";
    for (size_t i = 0; i < node->n_kids; i++) {
      IrNode *c = node->kids[i];
      if (c->kind == IR_ATTR && c->name &&
          (strcmp(c->name, "to") == 0 || strcmp(c->name, "href") == 0) &&
          c->value)
        to = c->value;
    }
    if (!url_href_is_safe(to)) to = "/";
    char *esc = js_escape_dq_dup(to);
    if (to[0] == '/')
      sb_appendf(sb, " href=\"#%s\"", esc ? esc : "/");
    else
      sb_appendf(sb, " href=\"#/%s\"", esc ? esc : "/");
    free(esc);
  }

  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *c = node->kids[i];
    if (c->kind != IR_ATTR || !c->name) continue;
    if (c->name[0] == '_' && c->name[1] == '_') continue;
    if (is_style_attr(c->name)) continue;
    if (strcmp(c->name, "to") == 0 || strcmp(c->name, "href") == 0) continue;
    /* bool class flags already in class= */
    if (attr_is_true(c) &&
        (strcmp(c->name, "between") == 0 || strcmp(c->name, "center") == 0 ||
         strcmp(c->name, "bold") == 0 || strcmp(c->name, "muted") == 0 ||
         strcmp(c->name, "sticky") == 0 || strcmp(c->name, "primary") == 0 ||
         strcmp(c->name, "outline") == 0 || strcmp(c->name, "ghost") == 0 ||
         strcmp(c->name, "font-mono") == 0 || strcmp(c->name, "flex-1") == 0 ||
         strcmp(c->name, "border") == 0 ||
         strcmp(c->name, "xl") == 0 || strcmp(c->name, "2xl") == 0 ||
         strcmp(c->name, "4xl") == 0 || strcmp(c->name, "lg") == 0 ||
         strcmp(c->name, "sm") == 0 || strcmp(c->name, "3xl") == 0 ||
         strcmp(c->name, "xs") == 0 || strcmp(c->name, "required") == 0 ||
         strcmp(c->name, "disabled") == 0 || strcmp(c->name, "text") == 0 ||
         strcmp(c->name, "email") == 0 || strcmp(c->name, "password") == 0 ||
         strcmp(c->name, "search") == 0))
      continue;
    if (strcmp(c->name, "bind") == 0 && c->value) {
      sb_appendf(sb, " bind:value={%s}", c->value);
      {
        int has_name = 0;
        for (size_t j = 0; j < node->n_kids; j++) {
          IrNode *a = node->kids[j];
          if (a->kind == IR_ATTR && a->name && strcmp(a->name, "name") == 0)
            has_name = 1;
        }
        if (!has_name && !strchr(c->value, '.'))
          sb_appendf(sb, " name=\"%s\"", c->value);
      }
      continue;
    }
    if (strcmp(c->name, "ref") == 0 && c->value) {
      sb_appendf(sb, " bind:this={%s}", c->value);
      continue;
    }
    if (strcmp(c->name, "action") == 0 && c->value) {
      sb_appendf(sb, " onsubmit={%s}", c->value);
      continue;
    }
    if (is_svelte_directive_attr(c->name) && c->value) {
      emit_svelte_directive(sb, c->name, c->value);
      continue;
    }
    if (strcmp(c->name, "src") == 0 || strcmp(c->name, "alt") == 0 ||
        strcmp(c->name, "placeholder") == 0 || strcmp(c->name, "type") == 0 ||
        strcmp(c->name, "name") == 0 || strcmp(c->name, "id") == 0 ||
        strcmp(c->name, "rows") == 0) {
      if (c->value && looks_like_js_expr(c->value) && strchr(c->value, '.'))
        sb_appendf(sb, " %s={%s}", c->name, c->value);
      else {
        char *esc = js_escape_dq_dup(c->value ? c->value : "");
        sb_appendf(sb, " %s=\"%s\"", c->name, esc ? esc : "");
        free(esc);
      }
    }
  }

  if (strcmp(tag, "checkbox") == 0) sb_append(sb, " type=\"checkbox\"");
  if (strcmp(tag, "input") == 0) {
    int has_type = 0;
    for (size_t i = 0; i < node->n_kids; i++) {
      IrNode *c = node->kids[i];
      if (c->kind == IR_ATTR && c->name && strcmp(c->name, "type") == 0)
        has_type = 1;
      if (c->kind == IR_ATTR && c->name && attr_is_true(c)) {
        if (strcmp(c->name, "text") == 0 || strcmp(c->name, "email") == 0 ||
            strcmp(c->name, "password") == 0 ||
            strcmp(c->name, "search") == 0) {
          if (!has_type) {
            sb_appendf(sb, " type=\"%s\"", c->name);
            has_type = 1;
          }
        }
      }
    }
  }

  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *c = node->kids[i];
    if (c->kind == IR_EVENT && c->name && c->value) {
      int needs_arrow = strchr(c->value, '(') || strchr(c->value, '+') ||
                        strchr(c->value, '-') || strchr(c->value, ' ');
      if (strcmp(c->name, "submit") == 0) {
        if (needs_arrow)
          sb_appendf(sb, " onsubmit={(e) => { e.preventDefault(); %s; }}",
                     c->value);
        else
          sb_appendf(sb, " onsubmit={(e) => { e.preventDefault(); %s(e); }}",
                     c->value);
      } else if (needs_arrow) {
        sb_appendf(sb, " on%s={() => %s}", c->name, c->value);
      } else {
        sb_appendf(sb, " on%s={%s}", c->name, c->value);
      }
    }
  }

  if (self_close) {
    sb_append(sb, " />\n");
    return;
  }
  sb_append(sb, ">\n");

  /* implicit expr children from unknown bool attrs (legacy parity) */
  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *c = node->kids[i];
    if (c->kind == IR_ATTR && c->name && attr_is_true(c)) {
      const char *v = c->name;
      if (strcmp(v, "between") && strcmp(v, "center") && strcmp(v, "bold") &&
          strcmp(v, "muted") && strcmp(v, "sticky") && strcmp(v, "primary") &&
          strcmp(v, "outline") && strcmp(v, "ghost") && strcmp(v, "required") &&
          strcmp(v, "disabled") && strcmp(v, "text") && strcmp(v, "email") &&
          strcmp(v, "search") && strcmp(v, "password") && strcmp(v, "xl") &&
          strcmp(v, "2xl") && strcmp(v, "3xl") && strcmp(v, "4xl") &&
          strcmp(v, "lg") && strcmp(v, "sm") && strcmp(v, "xs") &&
          !(v[0] == '_' && v[1] == '_')) {
        sb_indent(sb, depth + 1);
        sb_appendf(sb, "{%s}\n", v);
      }
    }
  }

  gen_children_ir(sb, node, depth + 1, is_layout);
  sb_indent(sb, depth);
  sb_appendf(sb, "</%s>\n", html);
}

static int is_ir_script_decl(const IrNode *n) {
  if (!n) return 1;
  if (n->kind == IR_PROP || n->kind == IR_STATE || n->kind == IR_COMPUTED ||
      n->kind == IR_EFFECT || n->kind == IR_FETCH || n->kind == IR_STORE ||
      n->kind == IR_MODULE_USE || n->kind == IR_ATTR || n->kind == IR_EVENT)
    return 1;
  if (n->kind == IR_HOOK && n->name) {
    /* markup hooks */
    if (strcmp(n->name, "portal") == 0 || strcmp(n->name, "head") == 0 ||
        strcmp(n->name, "empty") == 0 || strcmp(n->name, "loading") == 0 ||
        strcmp(n->name, "suspense") == 0 ||
        strcmp(n->name, "errorBoundary") == 0)
      return 0;
    return 1;
  }
  return 0;
}

static void gen_children_ir(StrBuf *sb, IrNode *node, int depth, int is_layout) {
  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *child = node->kids[i];
    if (!child) continue;
    if (child->kind == IR_FOR) {
      const char *var = child->name ? child->name : "item";
      const char *list = child->value ? child->value : "items";
      const char *key = ir_attr(child, "key");
      sb_indent(sb, depth);
      if (key)
        sb_appendf(sb, "{#each %s || [] as %s (%s)}\n", list, var, key);
      else
        sb_appendf(sb, "{#each %s || [] as %s}\n", list, var);
      for (size_t j = 0; j < child->n_kids; j++) {
        IrNode *ch = child->kids[j];
        if (ch->kind == IR_ATTR || ch->kind == IR_EVENT) continue;
        gen_node_ir(sb, ch, depth + 1, is_layout);
      }
      sb_indent(sb, depth);
      sb_append(sb, "{/each}\n");
    } else if (child->kind == IR_IF) {
      const char *cond = child->value ? child->value : "true";
      int has_else = 0;
      size_t else_idx = 0;
      for (size_t j = i + 1; j < node->n_kids; j++) {
        if (node->kids[j]->kind == IR_TEXT && node->kids[j]->value &&
            strcmp(node->kids[j]->value, "__else__") == 0) {
          has_else = 1;
          else_idx = j;
          break;
        }
      }
      sb_indent(sb, depth);
      sb_appendf(sb, "{#if %s}\n", cond);
      for (size_t j = 0; j < child->n_kids; j++)
        gen_node_ir(sb, child->kids[j], depth + 1, is_layout);
      if (has_else) {
        sb_indent(sb, depth);
        sb_append(sb, "{:else}\n");
        IrNode *en = node->kids[else_idx];
        for (size_t j = 0; j < en->n_kids; j++)
          gen_node_ir(sb, en->kids[j], depth + 1, is_layout);
        i = else_idx;
      }
      sb_indent(sb, depth);
      sb_append(sb, "{/if}\n");
    } else if (child->kind == IR_TEXT || child->kind == IR_INTERP) {
      gen_text_or_interp_ir(sb, child, depth);
    } else if (child->kind == IR_SLOT) {
      sb_indent(sb, depth);
      sb_append(sb, "{@render children?.()}\n");
    } else if (irw_hook_is(child, "empty")) {
      const char *cond = child->value ? child->value : "true";
      sb_indent(sb, depth);
      sb_appendf(sb, "{#if %s}\n", cond);
      for (size_t j = 0; j < child->n_kids; j++)
        gen_node_ir(sb, child->kids[j], depth + 1, is_layout);
      sb_indent(sb, depth);
      sb_append(sb, "{/if}\n");
    } else if (irw_hook_is(child, "loading") || irw_hook_is(child, "suspense")) {
      for (size_t j = 0; j < child->n_kids; j++) {
        IrNode *ch = child->kids[j];
        if (ch->kind == IR_ATTR) continue;
        if (ch->kind == IR_ELEMENT && ch->name &&
            strcmp(ch->name, "__fallback__") == 0)
          continue;
        gen_node_ir(sb, ch, depth, is_layout);
      }
    } else if (irw_hook_is(child, "head")) {
      if (child->value) {
        char title_e[512];
        html_escape_to(title_e, sizeof(title_e), child->value);
        sb_indent(sb, depth);
        sb_append(sb, "<svelte:head>\n");
        sb_indent(sb, depth + 1);
        sb_appendf(sb, "<title>%s</title>\n", title_e);
        sb_indent(sb, depth);
        sb_append(sb, "</svelte:head>\n");
      }
    } else if (child->kind == IR_AWAIT || child->kind == IR_SNIPPET ||
               child->kind == IR_RENDER || irw_hook_is(child, "portal")) {
      gen_node_ir(sb, child, depth, is_layout);
    } else if (!is_ir_script_decl(child) && child->kind != IR_COMPONENT &&
               child->kind != IR_LAYOUT && child->kind != IR_ROUTE) {
      gen_node_ir(sb, child, depth, is_layout);
    }
  }
}

static void gen_node_ir(StrBuf *sb, IrNode *node, int depth, int is_layout) {
  if (!node) return;
  switch (node->kind) {
    case IR_PROJECT:
      gen_children_ir(sb, node, depth, is_layout);
      break;
    case IR_ELEMENT:
      gen_element_ir(sb, node, depth, is_layout);
      break;
    case IR_SLOT:
      sb_indent(sb, depth);
      sb_append(sb, "{@render children?.()}\n");
      break;
    case IR_TEXT:
    case IR_INTERP:
      gen_text_or_interp_ir(sb, node, depth);
      break;
    case IR_HOOK:
      if (node->name && strcmp(node->name, "empty") == 0) {
        const char *cond = node->value ? node->value : "true";
        sb_indent(sb, depth);
        sb_appendf(sb, "{#if %s}\n", cond);
        for (size_t i = 0; i < node->n_kids; i++)
          gen_node_ir(sb, node->kids[i], depth + 1, is_layout);
        sb_indent(sb, depth);
        sb_append(sb, "{/if}\n");
      } else if (node->name && (strcmp(node->name, "loading") == 0 ||
                                strcmp(node->name, "suspense") == 0)) {
        for (size_t i = 0; i < node->n_kids; i++) {
          IrNode *ch = node->kids[i];
          if (ch->kind == IR_ATTR) continue;
          if (ch->kind == IR_ELEMENT && ch->name &&
              strcmp(ch->name, "__fallback__") == 0)
            continue;
          gen_node_ir(sb, ch, depth, is_layout);
        }
      } else if (node->name && strcmp(node->name, "head") == 0) {
        if (node->value) {
          char title_e[512];
          html_escape_to(title_e, sizeof(title_e), node->value);
          sb_indent(sb, depth);
          sb_append(sb, "<svelte:head>\n");
          sb_indent(sb, depth + 1);
          sb_appendf(sb, "<title>%s</title>\n", title_e);
          sb_indent(sb, depth);
          sb_append(sb, "</svelte:head>\n");
        }
      } else if (node->name && strcmp(node->name, "portal") == 0) {
        const char *target = node->value ? node->value : "document.body";
        sb_indent(sb, depth);
        sb_appendf(sb, "<div use:portal={%s}>\n", target);
        for (size_t i = 0; i < node->n_kids; i++) {
          IrNode *ch = node->kids[i];
          if (ch->kind == IR_ATTR || ch->kind == IR_EVENT) continue;
          gen_node_ir(sb, ch, depth + 1, is_layout);
        }
        sb_indent(sb, depth);
        sb_append(sb, "</div>\n");
      } else {
        gen_children_ir(sb, node, depth, is_layout);
      }
      break;
    case IR_AWAIT: {
      const char *prom = node->value ? node->value : "Promise.resolve(null)";
      const char *then_n = node->name ? node->name : "value";
      const char *catch_n = "error";
      IrNode *loading = NULL, *err_block = NULL;
      for (size_t i = 0; i < node->n_kids; i++) {
        IrNode *ch = node->kids[i];
        if (ch->kind == IR_ATTR && ch->name && strcmp(ch->name, "catch") == 0 &&
            ch->value)
          catch_n = ch->value;
        if (ch->kind == IR_ELEMENT && ch->name) {
          if (strcmp(ch->name, "__loading__") == 0) loading = ch;
          if (strcmp(ch->name, "__error__") == 0) err_block = ch;
        }
      }
      sb_indent(sb, depth);
      sb_appendf(sb, "{#await %s}\n", prom);
      if (loading) {
        for (size_t i = 0; i < loading->n_kids; i++)
          gen_node_ir(sb, loading->kids[i], depth + 1, is_layout);
      }
      sb_indent(sb, depth);
      sb_appendf(sb, "{:then %s}\n", then_n);
      for (size_t i = 0; i < node->n_kids; i++) {
        IrNode *ch = node->kids[i];
        if (ch->kind == IR_ATTR) continue;
        if (ch->kind == IR_ELEMENT && ch->name &&
            (strcmp(ch->name, "__loading__") == 0 ||
             strcmp(ch->name, "__error__") == 0))
          continue;
        gen_node_ir(sb, ch, depth + 1, is_layout);
      }
      if (err_block) {
        sb_indent(sb, depth);
        sb_appendf(sb, "{:catch %s}\n", catch_n);
        for (size_t i = 0; i < err_block->n_kids; i++)
          gen_node_ir(sb, err_block->kids[i], depth + 1, is_layout);
      }
      sb_indent(sb, depth);
      sb_append(sb, "{/await}\n");
      break;
    }
    case IR_SNIPPET: {
      const char *name = node->name ? node->name : "snippet";
      const char *params = node->value ? node->value : "";
      sb_indent(sb, depth);
      if (params[0])
        sb_appendf(sb, "{#snippet %s(%s)}\n", name, params);
      else
        sb_appendf(sb, "{#snippet %s()}\n", name);
      for (size_t i = 0; i < node->n_kids; i++) {
        IrNode *ch = node->kids[i];
        if (ch->kind == IR_ATTR || ch->kind == IR_EVENT) continue;
        gen_node_ir(sb, ch, depth + 1, is_layout);
      }
      sb_indent(sb, depth);
      sb_append(sb, "{/snippet}\n");
      break;
    }
    case IR_RENDER: {
      sb_indent(sb, depth);
      if (node->name && strchr(node->name, '(')) {
        sb_appendf(sb, "{@render %s}\n", node->name);
      } else if (node->name) {
        const char *arg = NULL;
        for (size_t i = 0; i < node->n_kids; i++) {
          IrNode *a = node->kids[i];
          if (a->kind == IR_ATTR && a->value) {
            arg = a->value;
            break;
          }
        }
        if (arg)
          sb_appendf(sb, "{@render %s(\"%s\")}\n", node->name, arg);
        else
          sb_appendf(sb, "{@render %s?.()}\n", node->name);
      }
      break;
    }
    default:
      gen_children_ir(sb, node, depth, is_layout);
      break;
  }
}

static void sv_collect_binds_ir(IrNode *n, char names[][64], int *count,
                                int max) {
  if (!n || *count >= max) return;
  if (n->kind == IR_ATTR && n->name && strcmp(n->name, "bind") == 0 &&
      n->value && !strchr(n->value, '.')) {
    int found = 0;
    for (int i = 0; i < *count; i++) {
      if (strcmp(names[i], n->value) == 0) {
        found = 1;
        break;
      }
    }
    if (!found && *count < max) {
      snprintf(names[*count], sizeof(names[0]), "%s", n->value);
      (*count)++;
    }
  }
  for (size_t i = 0; i < n->n_kids; i++)
    sv_collect_binds_ir(n->kids[i], names, count, max);
}

static int sv_state_exists_ir(IrNode *def, const char *name) {
  if (!def || !name) return 0;
  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (c->kind == IR_STATE && c->name && strcmp(c->name, "__states__") != 0 &&
        strcmp(c->name, name) == 0)
      return 1;
    if (c->kind == IR_STATE && c->name && strcmp(c->name, "__states__") == 0) {
      for (size_t j = 0; j < c->n_kids; j++) {
        if (c->kids[j]->name && strcmp(c->kids[j]->name, name) == 0) return 1;
      }
    }
    if (irw_hook_is(c, "action") && c->value && strcmp(c->value, name) == 0)
      return 1;
    if (c->kind == IR_FETCH && c->name && strcmp(c->name, name) == 0) return 1;
  }
  return 0;
}

static void emit_js_lit(StrBuf *sb, const char *val) {
  if (!val) {
    sb_append(sb, "null");
    return;
  }
  if (irw_looks_number(val) || irw_looks_bool(val)) {
    sb_append(sb, val);
    return;
  }
  char *esc = js_escape_sq_dup(val);
  sb_appendf(sb, "'%s'", esc ? esc : "");
  free(esc);
}

static void foreach_state(IrNode *def,
                          void (*fn)(const char *name, const char *init,
                                     void *ud),
                          void *ud) {
  if (!def) return;
  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (c->kind != IR_STATE) continue;
    if (c->name && strcmp(c->name, "__states__") == 0) {
      for (size_t j = 0; j < c->n_kids; j++) {
        IrNode *st = c->kids[j];
        if (st->kind == IR_STATE && st->name) fn(st->name, st->value, ud);
      }
    } else if (c->name) {
      fn(c->name, c->value, ud);
    }
  }
}

static void foreach_prop(IrNode *def,
                         void (*fn)(const char *name, const char *deflt,
                                    void *ud),
                         void *ud) {
  if (!def) return;
  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (c->kind != IR_PROP) continue;
    if (c->name && strcmp(c->name, "__props__") == 0) {
      for (size_t j = 0; j < c->n_kids; j++) {
        IrNode *pr = c->kids[j];
        if (pr->kind == IR_PROP && pr->name) fn(pr->name, pr->value, ud);
      }
    } else if (c->name && strcmp(c->name, "__props__") != 0) {
      fn(c->name, c->value, ud);
    }
  }
}

typedef struct {
  StrBuf *script;
  int n;
  IrNode *props_bag; /* unused, for prop-dup check via names list */
  char prop_names[64][64];
  int n_prop_names;
} PropsEmitCtx;

static void props_collect_name(const char *name, const char *deflt, void *ud) {
  (void)deflt;
  PropsEmitCtx *ctx = ud;
  if (!name || ctx->n_prop_names >= 64) return;
  snprintf(ctx->prop_names[ctx->n_prop_names], sizeof(ctx->prop_names[0]), "%s",
           name);
  ctx->n_prop_names++;
}

static void props_emit_one(const char *name, const char *deflt, void *ud) {
  PropsEmitCtx *ctx = ud;
  if (!name) return;
  if (ctx->n++) sb_append(ctx->script, ", ");
  sb_append(ctx->script, name);
  if (deflt) {
    sb_append(ctx->script, " = ");
    emit_js_lit(ctx->script, deflt);
  }
}

typedef struct {
  StrBuf *script;
} StateEmitCtx;

static void state_emit_one(const char *name, const char *init, void *ud) {
  StateEmitCtx *ctx = ud;
  if (!name) return;
  sb_appendf(ctx->script, "  let %s = $state(", name);
  emit_js_lit(ctx->script, init ? init : "null");
  sb_append(ctx->script, ");\n");
}

static void state_emit_setter(const char *name, const char *init, void *ud) {
  (void)init;
  StateEmitCtx *ctx = ud;
  if (!name || !*name) return;
  char setter[128];
  snprintf(setter, sizeof(setter), "set%c%s",
           (char)toupper((unsigned char)name[0]), name + 1);
  sb_appendf(ctx->script,
             "  function %s(v) { %s = typeof v === 'function' ? v(%s) : v; "
             "}\n",
             setter, name, name);
}

/* Full .svelte module from IR */
static char *gen_svelte_module_ir(SvelteProject *proj, SvelteUnit *u) {
  IrNode *def = u->ir_def;
  if (!def) return strdup("<!-- empty -->\n");
  int is_layout = (u->kind == SK_LAYOUT);
  StrBuf script;
  sb_init(&script);
  StrBuf markup;
  sb_init(&markup);

  sv_reset_stores();

  SvModuleNeeds needs;
  memset(&needs, 0, sizeof(needs));
  scan_module_needs_ir(def, &needs);

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (c->kind == IR_STORE && c->name) sv_add_store(c->name);
  }

  ProvideInfo provides[16];
  int n_provides = 0;
  collect_provides_ir(def, provides, &n_provides, 16);

  int need_set_ctx = (n_provides > 0);
  int need_get_ctx = 0;
  for (size_t i = 0; i < def->n_kids; i++) {
    if (irw_hook_is(def->kids[i], "ctx")) need_get_ctx = 1;
  }
  if (need_set_ctx || need_get_ctx) u->use_context = 1;

  if (need_set_ctx && need_get_ctx)
    sb_append(&script, "  import { setContext, getContext } from 'svelte';\n");
  else if (need_set_ctx)
    sb_append(&script, "  import { setContext } from 'svelte';\n");
  else if (need_get_ctx)
    sb_append(&script, "  import { getContext } from 'svelte';\n");

  if (needs.need_writable)
    sb_append(&script, "  import { writable } from 'svelte/store';\n");

  if (needs.n_transition > 0) {
    sb_append(&script, "  import { ");
    for (int i = 0; i < needs.n_transition; i++) {
      if (i) sb_append(&script, ", ");
      sb_append(&script, needs.transition_fns[i]);
    }
    sb_append(&script, " } from 'svelte/transition';\n");
  }
  if (needs.n_animate > 0) {
    sb_append(&script, "  import { ");
    for (int i = 0; i < needs.n_animate; i++) {
      if (i) sb_append(&script, ", ");
      sb_append(&script, needs.animate_fns[i]);
    }
    sb_append(&script, " } from 'svelte/animate';\n");
  }

  if (proj->n_contexts > 0 && (need_set_ctx || need_get_ctx || u->use_context)) {
    sb_append(&script, "  import { ");
    for (int i = 0; i < proj->n_contexts; i++) {
      if (i) sb_append(&script, ", ");
      sb_append(&script,
                proj->contexts[i]->value ? proj->contexts[i]->value : "Ctx");
    }
    sb_append(&script, " } from '../contexts.js';\n");
  }

  for (int i = 0; i < u->n_used; i++) {
    SvelteUnit *dep = find_unit(proj, u->used[i]);
    if (!dep) continue;
    if (strcmp(u->dir, dep->dir) == 0)
      sb_appendf(&script, "  import %s from './%s.svelte';\n", dep->name,
                 dep->name);
    else
      sb_appendf(&script, "  import %s from '../%s/%s.svelte';\n", dep->name,
                 dep->dir, dep->name);
  }

  IrNode *params_decl = ir_find_hook(def, "params");
  if (params_decl) u->use_params = 1;

  int has_props = 0;
  for (size_t i = 0; i < def->n_kids; i++) {
    if (def->kids[i]->kind == IR_PROP) {
      has_props = 1;
      break;
    }
  }

  int need_children = u->use_slot || is_layout;
  if (has_props || need_children || params_decl) {
    PropsEmitCtx pctx;
    memset(&pctx, 0, sizeof(pctx));
    pctx.script = &script;
    sb_append(&script, "  let { ");
    foreach_prop(def, props_emit_one, &pctx);
    /* remember prop names for params dup check */
    foreach_prop(def, props_collect_name, &pctx);

    if (params_decl && params_decl->value) {
      const char *p = params_decl->value;
      while (*p) {
        while (*p == ' ' || *p == ',') p++;
        if (!*p) break;
        char name[64];
        size_t ni = 0;
        while (*p && *p != ',' && *p != ' ' && ni + 1 < sizeof(name))
          name[ni++] = *p++;
        name[ni] = '\0';
        if (ni == 0) continue;
        int dup = 0;
        for (int k = 0; k < pctx.n_prop_names; k++) {
          if (strcmp(pctx.prop_names[k], name) == 0) dup = 1;
        }
        if (!dup) {
          if (pctx.n++) sb_append(&script, ", ");
          sb_append(&script, name);
        }
      }
    }
    if (need_children) {
      if (pctx.n++) sb_append(&script, ", ");
      sb_append(&script, "children");
    }
    sb_append(&script, " } = $props();\n");
  }

  {
    StateEmitCtx sctx = {.script = &script};
    foreach_state(def, state_emit_one, &sctx);
  }

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (c->kind == IR_COMPUTED && c->name) {
      sb_appendf(&script, "  let %s = $derived(%s);\n", c->name,
                 c->value ? c->value : "null");
    }
  }

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (irw_hook_is(c, "ref") && c->value)
      sb_appendf(&script, "  let %s = $state(null);\n", c->value);
  }

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (irw_hook_is(c, "ctx") && c->value && c->value2) {
      sb_appendf(&script, "  const __ctx_%s = getContext(%s);\n", c->value,
                 c->value2);
      sb_appendf(&script,
                 "  let %s = $derived(__ctx_%s != null && typeof __ctx_%s === "
                 "'object' && 'value' in __ctx_%s ? __ctx_%s.value : __ctx_%s);\n",
                 c->value, c->value, c->value, c->value, c->value, c->value);
    }
  }

  for (int i = 0; i < n_provides; i++) {
    const char *ctx_name =
        provides[i].ctx_name ? provides[i].ctx_name : "Context";
    const char *val = provides[i].val;
    sb_appendf(&script, "  setContext(%s, { get value() { return ", ctx_name);
    if (val) {
      if (irw_looks_number(val) || irw_looks_bool(val) ||
          (looks_like_js_expr(val) && (strchr(val, '.') || strchr(val, '('))))
        sb_append(&script, val);
      else if (looks_like_js_expr(val))
        sb_append(&script, val);
      else
        emit_js_lit(&script, val);
    } else {
      sb_append(&script, "undefined");
    }
    sb_append(&script, "; } });\n");
  }

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (c->kind != IR_STORE || !c->name) continue;
    sb_appendf(&script, "  const %s = writable(", c->name);
    if (c->value)
      emit_js_lit(&script, c->value);
    else
      sb_append(&script, "null");
    sb_append(&script, ");\n");
    char setter[128];
    snprintf(setter, sizeof(setter), "set%c%s",
             (char)toupper((unsigned char)c->name[0]), c->name + 1);
    sb_appendf(&script, "  function %s(v) { %s.set(v); }\n", setter, c->name);
  }

  if (needs.need_portal) {
    sb_append(&script,
              "  function portal(node, target) {\n"
              "    const t = target || (typeof document !== 'undefined' ? "
              "document.body : null);\n"
              "    if (t) t.appendChild(node);\n"
              "    return {\n"
              "      update(newTarget) {\n"
              "        const nt = newTarget || (typeof document !== 'undefined' "
              "? document.body : null);\n"
              "        if (nt) nt.appendChild(node);\n"
              "      },\n"
              "      destroy() { node.remove(); }\n"
              "    };\n"
              "  }\n");
  }

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (irw_hook_is(c, "id") && c->value)
      sb_appendf(&script, "  const %s = $props.id();\n", c->value);
  }

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (c->kind != IR_EFFECT || !c->value2) continue;
    /* only effect / layoutEffect — skip insertionEffect */
    if (c->name && strcmp(c->name, "insertionEffect") == 0) continue;
    if (c->name && strcmp(c->name, "effect") != 0 &&
        strcmp(c->name, "layoutEffect") != 0)
      continue;
    const char *hook =
        (c->name && strcmp(c->name, "layoutEffect") == 0) ? "$effect.pre"
                                                          : "$effect";
    sb_appendf(&script, "  %s(() => {\n", hook);
    sb_appendf(&script, "    %s;\n", c->value2);
    for (size_t j = 0; j < c->n_kids; j++) {
      if (c->kids[j]->kind == IR_ATTR && c->kids[j]->name &&
          strcmp(c->kids[j]->name, "cleanup") == 0 && c->kids[j]->value) {
        sb_append(&script, "    return () => {\n");
        sb_appendf(&script, "      %s;\n", c->kids[j]->value);
        sb_append(&script, "    };\n");
      }
    }
    sb_append(&script, "  });\n");
  }

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (!irw_hook_is(c, "action") || !c->value) continue;
    const char *st = c->value;
    const char *fn = c->value2 ? c->value2 : "async () => null";
    const char *init = "null";
    const char *pending = NULL;
    for (size_t j = 0; j < c->n_kids; j++) {
      if (c->kids[j]->kind != IR_ATTR || !c->kids[j]->name) continue;
      if (strcmp(c->kids[j]->name, "init") == 0 && c->kids[j]->value)
        init = c->kids[j]->value;
      if (strcmp(c->kids[j]->name, "pending") == 0 && c->kids[j]->value)
        pending = c->kids[j]->value;
    }
    char pending_name[128], action_name[128];
    if (pending)
      snprintf(pending_name, sizeof(pending_name), "%s", pending);
    else
      snprintf(pending_name, sizeof(pending_name), "%sPending", st);
    snprintf(action_name, sizeof(action_name), "%sAction", st);

    sb_appendf(&script, "  let %s = $state(%s);\n", st, init);
    sb_appendf(&script, "  let %s = $state(false);\n", pending_name);
    sb_appendf(&script, "  async function %s(e) {\n", action_name);
    sb_append(&script, "    e.preventDefault();\n");
    sb_appendf(&script, "    %s = true;\n", pending_name);
    sb_append(&script, "    try {\n");
    sb_appendf(&script, "      %s = await %s(%s, new FormData(e.target));\n", st,
               fn, st);
    sb_append(&script, "    } finally {\n");
    sb_appendf(&script, "      %s = false;\n", pending_name);
    sb_append(&script, "    }\n");
    sb_append(&script, "  }\n");
  }

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (c->kind != IR_FETCH || !c->name) continue;
    const char *nm = c->name;
    const char *url = c->value ? c->value : "/";
    sb_appendf(&script, "  let %s = $state(null);\n", nm);
    sb_appendf(&script, "  let %sError = $state(null);\n", nm);
    sb_appendf(&script, "  let %sLoading = $state(true);\n", nm);
    sb_append(&script, "  $effect(() => {\n");
    sb_append(&script, "    let cancelled = false;\n");
    sb_appendf(&script, "    %sLoading = true;\n", nm);
    sb_appendf(&script, "    %sError = null;\n", nm);
    {
      const char *safe_url = url_href_is_safe(url) ? url : "/";
      char *esc = js_escape_dq_dup(safe_url);
      sb_appendf(&script, "    fetch(\"%s\")\n", esc ? esc : "/");
      free(esc);
    }
    sb_append(&script, "      .then((r) => {\n");
    sb_append(&script, "        if (!r.ok) throw new Error(String(r.status));\n");
    sb_append(&script, "        return r.json();\n");
    sb_append(&script, "      })\n");
    sb_appendf(&script,
               "      .then((data) => { if (!cancelled) %s = data; })\n", nm);
    sb_appendf(&script,
               "      .catch((err) => { if (!cancelled) %sError = err; })\n",
               nm);
    sb_appendf(&script,
               "      .finally(() => { if (!cancelled) %sLoading = false; });\n",
               nm);
    sb_append(&script, "    return () => { cancelled = true; };\n");
    sb_append(&script, "  });\n");
  }

  {
    char binds[32][64];
    int n_binds = 0;
    sv_collect_binds_ir(def, binds, &n_binds, 32);
    for (int bi = 0; bi < n_binds; bi++) {
      if (sv_state_exists_ir(def, binds[bi])) continue;
      if (sv_is_store_expr(binds[bi])) continue;
      sb_appendf(&script, "  let %s = $state('');\n", binds[bi]);
    }
  }

  {
    StateEmitCtx sctx = {.script = &script};
    foreach_state(def, state_emit_setter, &sctx);
  }

  {
    char binds[32][64];
    int n_binds = 0;
    sv_collect_binds_ir(def, binds, &n_binds, 32);
    for (int bi = 0; bi < n_binds; bi++) {
      if (sv_state_exists_ir(def, binds[bi])) {
        int has_explicit = 0;
        for (size_t i = 0; i < def->n_kids; i++) {
          IrNode *c = def->kids[i];
          if (c->kind == IR_STATE && c->name &&
              strcmp(c->name, "__states__") != 0 &&
              strcmp(c->name, binds[bi]) == 0)
            has_explicit = 1;
          if (c->kind == IR_STATE && c->name &&
              strcmp(c->name, "__states__") == 0) {
            for (size_t j = 0; j < c->n_kids; j++) {
              if (c->kids[j]->name &&
                  strcmp(c->kids[j]->name, binds[bi]) == 0)
                has_explicit = 1;
            }
          }
        }
        if (has_explicit) continue;
      }
      char setter[128];
      snprintf(setter, sizeof(setter), "set%c%s",
               (char)toupper((unsigned char)binds[bi][0]), binds[bi] + 1);
      sb_appendf(&script,
                 "  function %s(v) { %s = typeof v === 'function' ? v(%s) : v; "
                 "}\n",
                 setter, binds[bi], binds[bi]);
    }
  }

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (is_ir_script_decl(c)) continue;
    gen_node_ir(&markup, c, 0, is_layout);
  }

  StrBuf out;
  sb_init(&out);
  if (u->source_path[0])
    sb_appendf(&out, "<!-- cordlang: source=%s -->\n", u->source_path);
  sb_append(&out, "<script>\n");
  sb_append(&out, script.buf ? script.buf : "");
  sb_append(&out, "</script>\n\n");
  sb_append(&out, markup.buf ? markup.buf : "");
  free(script.buf);
  free(markup.buf);
  return out.buf;
}

static char *gen_contexts_module_ir(SvelteProject *proj) {
  if (!proj || proj->n_contexts == 0) return NULL;
  StrBuf out;
  sb_init(&out);
  sb_append(&out, "/* Cordlang context keys — use with setContext/getContext */\n");
  for (int i = 0; i < proj->n_contexts; i++) {
    IrNode *c = proj->contexts[i];
    const char *name = c->value ? c->value : "AppContext";
    sb_appendf(&out, "export const %s = Symbol('%s');\n", name, name);
  }
  return out.buf;
}

static const char *route_layout_attr_ir(IrNode *route) {
  return ir_attr(route, "layout");
}

static SvelteUnit *find_layout_for_route_ir(SvelteProject *proj, IrNode *route) {
  const char *attr = route_layout_attr_ir(route);
  char export_name[96];

  if (attr) {
    layout_name(attr, export_name, sizeof(export_name));
    for (int i = 0; i < proj->n_units; i++) {
      if (proj->units[i].kind != SK_LAYOUT) continue;
      if (strcmp(proj->units[i].name, export_name) == 0) return &proj->units[i];
      if (proj->units[i].ir_def && proj->units[i].ir_def->name &&
          strcmp(proj->units[i].ir_def->name, attr) == 0)
        return &proj->units[i];
    }
    return NULL;
  }

  SvelteUnit *first = NULL;
  for (int i = 0; i < proj->n_units; i++) {
    if (proj->units[i].kind != SK_LAYOUT) continue;
    if (!first) first = &proj->units[i];
    if (strcmp(proj->units[i].name, "DefaultLayout") == 0) return &proj->units[i];
    if (proj->units[i].ir_def && proj->units[i].ir_def->name &&
        strcmp(proj->units[i].ir_def->name, "default") == 0)
      return &proj->units[i];
  }
  return first;
}

static char *gen_app_svelte_ir(SvelteProject *proj) {
  StrBuf out;
  sb_init(&out);
  sb_append(&out, "<!-- cordlang: source=src/app.cord -->\n");
  sb_append(&out, "<script>\n");
  sb_append(&out, "  import { onMount } from 'svelte';\n");

  int n_layouts = 0;
  for (int i = 0; i < proj->n_units; i++) {
    if (proj->units[i].kind == SK_LAYOUT) {
      sb_appendf(&out, "  import %s from './layouts/%s.svelte';\n",
                 proj->units[i].name, proj->units[i].name);
      n_layouts++;
    }
  }

  int any_lazy = 0;
  for (int r = 0; r < proj->n_routes; r++) {
    if (route_is_lazy_ir(proj, proj->routes[r])) {
      any_lazy = 1;
      break;
    }
  }

  int imported[SV_MAX_UNITS];
  memset(imported, 0, sizeof(imported));

  for (int r = 0; r < proj->n_routes; r++) {
    const char *comp = proj->routes[r]->value;
    if (!comp || route_is_lazy_ir(proj, proj->routes[r])) continue;
    for (int i = 0; i < proj->n_units; i++) {
      if (imported[i]) continue;
      if (strcmp(proj->units[i].name, comp) == 0) {
        sb_appendf(&out, "  import %s from './%s/%s.svelte';\n",
                   proj->units[i].name, proj->units[i].dir, proj->units[i].name);
        imported[i] = 1;
        break;
      }
    }
  }

  for (int r = 0; r < proj->n_routes; r++) {
    const char *comp = proj->routes[r]->value;
    if (!comp || !route_is_lazy_ir(proj, proj->routes[r])) continue;
    int already = 0;
    for (int prev = 0; prev < r; prev++) {
      if (proj->routes[prev]->value &&
          strcmp(proj->routes[prev]->value, comp) == 0 &&
          route_is_lazy_ir(proj, proj->routes[prev]))
        already = 1;
    }
    if (already) continue;
    const char *dir = "pages";
    for (int i = 0; i < proj->n_units; i++) {
      if (strcmp(proj->units[i].name, comp) == 0) {
        dir = proj->units[i].dir;
        imported[i] = 1;
        break;
      }
    }
    sb_appendf(&out,
               "  const %s = () => import('./%s/%s.svelte').then(m => m.default);\n",
               comp, dir, comp);
  }

  if (proj->n_routes > 0) {
    sb_append(&out, "\n  const routes = [\n");
    for (int r = 0; r < proj->n_routes; r++) {
      const char *path = proj->routes[r]->name ? proj->routes[r]->name : "/";
      const char *comp =
          proj->routes[r]->value ? proj->routes[r]->value : "HomePage";
      SvelteUnit *layout = find_layout_for_route_ir(proj, proj->routes[r]);
      int lazy = route_is_lazy_ir(proj, proj->routes[r]);
      if (any_lazy) {
        if (layout)
          sb_appendf(&out,
                     "    { path: '%s', component: %s, layout: %s, lazy: %s },\n",
                     path, comp, layout->name, lazy ? "true" : "false");
        else
          sb_appendf(&out,
                     "    { path: '%s', component: %s, layout: null, lazy: %s },\n",
                     path, comp, lazy ? "true" : "false");
      } else if (layout) {
        sb_appendf(&out, "    { path: '%s', component: %s, layout: %s },\n",
                   path, comp, layout->name);
      } else {
        sb_appendf(&out, "    { path: '%s', component: %s, layout: null },\n",
                   path, comp);
      }
    }
    sb_append(&out, "  ];\n\n");
    sb_append(&out,
              "  function currentPath() {\n"
              "    const raw = window.location.hash.replace(/^#/, '') || '/';\n"
              "    const h = raw.split('?')[0] || '/';\n"
              "    return h.startsWith('/') ? h : '/' + h;\n"
              "  }\n"
              "  function matchRoute(pathname) {\n"
              "    const segs = pathname.replace(/\\/+$/, '').split('/').filter(Boolean);\n"
              "    for (const r of routes) {\n"
              "      const pp = r.path.replace(/\\/+$/, '').split('/').filter(Boolean);\n"
              "      if (pp.length !== segs.length) continue;\n"
              "      const params = {};\n"
              "      let ok = true;\n"
              "      for (let i = 0; i < pp.length; i++) {\n"
              "        if (pp[i].startsWith(':')) params[pp[i].slice(1)] = decodeURIComponent(segs[i]);\n"
              "        else if (pp[i] !== segs[i]) { ok = false; break; }\n"
              "      }\n");
    if (any_lazy)
      sb_append(&out,
                "      if (ok) return { component: r.component, layout: r.layout, lazy: r.lazy, params };\n");
    else
      sb_append(&out,
                "      if (ok) return { component: r.component, layout: r.layout, params };\n");
    sb_append(&out,
              "    }\n"
              "    return null;\n"
              "  }\n"
              "  let path = $state(currentPath());\n"
              "  onMount(() => {\n"
              "    const onHash = () => { path = currentPath(); };\n"
              "    window.addEventListener('hashchange', onHash);\n"
              "    if (!window.location.hash) window.location.hash = '#/';\n"
              "    return () => window.removeEventListener('hashchange', onHash);\n"
              "  });\n"
              "  let matched = $derived(matchRoute(path) ?? matchRoute('/') ?? null);\n"
              "  let Page = $derived(matched?.component ?? null);\n"
              "  let Layout = $derived(matched?.layout ?? null);\n");
    if (any_lazy)
      sb_append(&out, "  let isLazy = $derived(matched?.lazy ?? false);\n");
    sb_append(&out, "  let params = $derived(matched?.params ?? {});\n");
  }

  sb_append(&out, "</script>\n\n");

  if (proj->n_routes > 0) {
    if (n_layouts > 0) {
      sb_append(&out, "{#if Layout && Page}\n");
      sb_append(&out, "  <Layout>\n");
      sb_append(&out, "    {#snippet children()}\n");
      if (any_lazy) {
        sb_append(&out,
                  "      {#if isLazy}\n"
                  "        {#await Page()}\n"
                  "          <p class=\"text-gray-500 p-4\">Cargando...</p>\n"
                  "        {:then Comp}\n"
                  "          <Comp {...params} />\n"
                  "        {:catch}\n"
                  "          <p class=\"text-red-500 p-4\">Error al cargar</p>\n"
                  "        {/await}\n"
                  "      {:else}\n"
                  "        <Page {...params} />\n"
                  "      {/if}\n");
      } else {
        sb_append(&out, "      <Page {...params} />\n");
      }
      sb_append(&out, "    {/snippet}\n");
      sb_append(&out, "  </Layout>\n");
      sb_append(&out, "{:else if Page}\n");
      if (any_lazy) {
        sb_append(&out,
                  "  {#if isLazy}\n"
                  "    {#await Page()}\n"
                  "      <p class=\"text-gray-500 p-4\">Cargando...</p>\n"
                  "    {:then Comp}\n"
                  "      <Comp {...params} />\n"
                  "    {:catch}\n"
                  "      <p class=\"text-red-500 p-4\">Error al cargar</p>\n"
                  "    {/await}\n"
                  "  {:else}\n"
                  "    <Page {...params} />\n"
                  "  {/if}\n");
      } else {
        sb_append(&out, "  <Page {...params} />\n");
      }
      sb_append(&out, "{/if}\n");
    } else {
      sb_append(&out, "{#if Page}\n");
      if (any_lazy) {
        sb_append(&out,
                  "  {#if isLazy}\n"
                  "    {#await Page()}\n"
                  "      <p class=\"text-gray-500 p-4\">Cargando...</p>\n"
                  "    {:then Comp}\n"
                  "      <Comp {...params} />\n"
                  "    {:catch}\n"
                  "      <p class=\"text-red-500 p-4\">Error al cargar</p>\n"
                  "    {/await}\n"
                  "  {:else}\n"
                  "    <Page {...params} />\n"
                  "  {/if}\n");
      } else {
        sb_append(&out, "  <Page {...params} />\n");
      }
      sb_append(&out, "{/if}\n");
    }
  } else if (proj->n_units > 0) {
    SvelteUnit *root = &proj->units[0];
    for (int i = 0; i < proj->n_units; i++) {
      if (proj->units[i].kind == SK_PAGE) {
        root = &proj->units[i];
        break;
      }
    }
    free(out.buf);
    sb_init(&out);
    sb_append(&out, "<!-- cordlang: source=src/app.cord -->\n");
    sb_append(&out, "<script>\n");
    sb_appendf(&out, "  import %s from './%s/%s.svelte';\n", root->name, root->dir,
               root->name);
    sb_append(&out, "</script>\n\n");
    sb_appendf(&out, "<%s />\n", root->name);
  } else {
    sb_append(&out, "<p>Empty Cordlang app</p>\n");
  }
  return out.buf;
}

int svelte_emit_modules_from_ir(IrProgram *ir, SvelteWriteFn write_fn,
                                void *userdata) {
  if (!ir || !ir->root) return -1;
  SvelteProject *proj = calloc(1, sizeof(SvelteProject));
  if (!proj) return -1;
  project_partition_from_ir(proj, ir->root);
  if (proj->truncated) {
    free(proj);
    return -1;
  }
  harvest_contexts_ir(proj, ir->root);

  if (proj->n_contexts > 0) {
    char *ctxm = gen_contexts_module_ir(proj);
    if (ctxm) {
      int rc = write_fn("contexts.js", ctxm, userdata);
      free(ctxm);
      if (rc != 0) {
        free(proj);
        return rc;
      }
    }
  }

  for (int i = 0; i < proj->n_units; i++) {
    char *mod = gen_svelte_module_ir(proj, &proj->units[i]);
    if (!mod) {
      free(proj);
      return -1;
    }
    int rc = write_fn(proj->units[i].rel, mod, userdata);
    free(mod);
    if (rc != 0) {
      free(proj);
      return rc;
    }
  }

  char *app = gen_app_svelte_ir(proj);
  if (!app) {
    free(proj);
    return -1;
  }
  int rc = write_fn("App.svelte", app, userdata);
  free(app);
  free(proj);
  return rc;
}

int svelte_emit_modules(Node *root, SvelteWriteFn write_fn, void *userdata) {
  if (!root) return -1;
  IrProgram *ir = ir_from_ast(root, NULL);
  if (!ir) return -1;
  int rc = svelte_emit_modules_from_ir(ir, write_fn, userdata);
  ir_free(ir);
  return rc;
}

static char *svelte_generate_impl_ir(IrProgram *ir) {
  if (!ir || !ir->root) return strdup("<!-- empty -->\n");
  StrBuf sb;
  sb_init(&sb);
  SvelteProject *proj = calloc(1, sizeof(SvelteProject));
  if (!proj) return strdup("<!-- empty -->\n");
  project_partition_from_ir(proj, ir->root);
  if (proj->truncated) {
    free(proj);
    return NULL;
  }
  harvest_contexts_ir(proj, ir->root);

  if (proj->n_contexts > 0) {
    char *ctxm = gen_contexts_module_ir(proj);
    sb_append(&sb, "<!-- ===== src/contexts.js ===== -->\n");
    sb_append(&sb, ctxm ? ctxm : "");
    sb_append(&sb, "\n");
    free(ctxm);
  }

  for (int i = 0; i < proj->n_units; i++) {
    char *mod = gen_svelte_module_ir(proj, &proj->units[i]);
    sb_appendf(&sb, "<!-- ===== src/%s ===== -->\n", proj->units[i].rel);
    sb_append(&sb, mod ? mod : "");
    sb_append(&sb, "\n");
    free(mod);
  }
  char *app = gen_app_svelte_ir(proj);
  sb_append(&sb, "<!-- ===== src/App.svelte ===== -->\n");
  sb_append(&sb, app ? app : "");
  free(app);
  free(proj);
  return sb.buf;
}

char *svelte_generate_from_ir(IrProgram *ir) {
  if (!ir || !ir->root) return strdup("<!-- empty -->\n");
  return svelte_generate_impl_ir(ir);
}

char *svelte_generate(Node *root) {
  if (!root) return strdup("<!-- empty -->\n");
  IrProgram *ir = ir_from_ast(root, NULL);
  if (!ir) return strdup("<!-- empty -->\n");
  char *out = svelte_generate_from_ir(ir);
  ir_free(ir);
  return out;
}
