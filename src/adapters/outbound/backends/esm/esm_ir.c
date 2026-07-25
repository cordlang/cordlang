/*
 * IR → ES module codegen, one module per .cord file.
 *
 * Unlike every other backend here, this one does NOT see a flattened project:
 * the dev server hands it the IR of a SINGLE file, so `use` / `route` targets
 * are still module refs. Those become real `import` statements pointing at other
 * .cord URLs, and the browser walks the graph itself.
 *
 * Emit shape, component module:
 *   import { h, component } from '/@cord/runtime.js';
 *   import SiteHeader from '/src/components/SiteHeader.cord';
 *   export default component('HomePage', function (props, $) { ... return h(...) });
 *
 * Emit shape, entry module:
 *   export const routes = [{ path: '/', component: HomePage, layout: DefaultLayout }];
 *   export default { __cord: 'app', routes, theme };
 */
#include "adapters/outbound/backends/esm/esm_backend.h"
#include "adapters/outbound/backends/cord_class.h"
#include "adapters/outbound/backends/theme_css.h"
#include "application/ports/compiler_port.h"
#include "application/ports/fs_port.h"
#include "adapters/outbound/html_escape.h"
#include "domain/interp.h"
#include "domain/ir.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── string buffer ──────────────────────────────────────── */

typedef struct {
  char *buf;
  size_t len;
  size_t cap;
} Sb;

static void sb_init(Sb *sb) {
  sb->cap = 16384;
  sb->len = 0;
  sb->buf = calloc(sb->cap, 1);
  if (!sb->buf) {
    fprintf(stderr, "fatal: out of memory (esm Sb)\n");
    exit(1);
  }
}

static void sb_grow(Sb *sb, size_t extra) {
  if (sb->len + extra + 1 < sb->cap) return;
  while (sb->len + extra + 1 >= sb->cap) {
    if (sb->cap > (size_t)-1 / 2) {
      fprintf(stderr, "fatal: out of memory (esm Sb)\n");
      exit(1);
    }
    sb->cap *= 2;
  }
  char *nb = realloc(sb->buf, sb->cap);
  if (!nb) {
    fprintf(stderr, "fatal: out of memory (esm Sb)\n");
    exit(1);
  }
  sb->buf = nb;
}

static void sb_add(Sb *sb, const char *s) {
  if (!s) return;
  size_t n = strlen(s);
  sb_grow(sb, n);
  memcpy(sb->buf + sb->len, s, n);
  sb->len += n;
  sb->buf[sb->len] = '\0';
}

static void sb_addf(Sb *sb, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(NULL, 0, fmt, ap);
  va_end(ap);
  if (n < 0) return;
  sb_grow(sb, (size_t)n);
  va_start(ap, fmt);
  vsnprintf(sb->buf + sb->len, sb->cap - sb->len, fmt, ap);
  va_end(ap);
  sb->len += (size_t)n;
  sb->buf[sb->len] = '\0';
}

static void sb_pad(Sb *sb, int depth) {
  for (int i = 0; i < depth; i++) sb_add(sb, "  ");
}

/* ── small helpers ──────────────────────────────────────── */

#define ESM_MAX_IMPORTS 128

typedef struct {
  char export_name[96];
  char url[512];
  char mod_path[512]; /* as written in source: layouts/docs */
  char basename[96];  /* docs */
} EsmImport;

#define ESM_MAX_SCOPE 192

typedef struct {
  const EsmModuleCtx *mod;
  EsmImport imports[ESM_MAX_IMPORTS];
  int n_imports;
  int is_layout;    /* current component is a layout → slot = props.children */
  int uses_children;
  int truncated;
  /*
   * Identifiers actually declared in the component being emitted (props, state,
   * computed, refs, fetch results, route params, `for` variables).
   *
   * Cord attribute values are untyped words: `purpose=action` and
   * `label=title` look exactly like `count` does. Guessing from the shape of
   * the word alone turns a semantic attribute into a variable reference and the
   * component dies with ReferenceError at render time. So an identifier is only
   * emitted as an expression when it is genuinely in scope; otherwise it is a
   * string.
   */
  char scope[ESM_MAX_SCOPE][64];
  int n_scope;
  /*
   * `use components/Nope` where the file is missing. Emitting nothing would
   * leave a dangling identifier and the whole page would die with a
   * ReferenceError; instead each one gets a stub component that renders the
   * problem where it happens, so the rest of the app still works.
   */
  struct {
    char export_name[96];
    char mod_path[256];
  } unresolved[32];
  int n_unresolved;
  /*
   * `foreign Chart from "recharts"` cannot resolve npm packages in the ESM
   * preview. Same degrade strategy as unresolved modules: a visible stub with
   * class `cord-runtime-error` so PascalCase `h(Chart, …)` binds to a real
   * const instead of crashing with ReferenceError.
   */
  struct {
    char name[96];
    char mod_path[256]; /* package hint for the comment; may be empty */
  } foreigns[32];
  int n_foreigns;
} EsmCtx;

static void scope_add(EsmCtx *c, const char *name) {
  if (!name || !*name || strlen(name) >= 64) return;
  if (c->n_scope >= ESM_MAX_SCOPE) return;
  for (int i = 0; i < c->n_scope; i++)
    if (strcmp(c->scope[i], name) == 0) return;
  snprintf(c->scope[c->n_scope], 64, "%s", name);
  c->n_scope++;
}

/* Root identifier of a value: `user.name` → `user`. */
static int scope_has(EsmCtx *c, const char *expr) {
  if (!c || !expr || !*expr) return 0;
  char root[64];
  size_t i = 0;
  for (; expr[i] && expr[i] != '.' && i < sizeof(root) - 1; i++) root[i] = expr[i];
  root[i] = '\0';
  for (int j = 0; j < c->n_scope; j++)
    if (strcmp(c->scope[j], root) == 0) return 1;
  return 0;
}

/* Comma/space separated declaration lists, e.g. `params id, slug`. */
static void scope_add_list(EsmCtx *c, const char *list) {
  if (!list) return;
  const char *p = list;
  while (*p) {
    while (*p == ' ' || *p == ',' || *p == '\t') p++;
    if (!*p) break;
    char buf[64];
    size_t n = 0;
    while (*p && *p != ' ' && *p != ',' && *p != '\t' && n < sizeof(buf) - 1)
      buf[n++] = *p++;
    buf[n] = '\0';
    scope_add(c, buf);
  }
}

static int is_pascal(const char *s) {
  return s && s[0] && isupper((unsigned char)s[0]);
}

static int is_safe_js_ident(const char *s) {
  if (!s || !*s) return 0;
  if (!(isalpha((unsigned char)s[0]) || s[0] == '_' || s[0] == '$')) return 0;
  for (const char *p = s; *p; p++)
    if (!(isalnum((unsigned char)*p) || *p == '_' || *p == '$')) return 0;
  return 1;
}

/* Dotted member access (user.name) — safe to emit as an expression. */
static int is_safe_js_path(const char *s) {
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

static void emit_sq_string(Sb *sb, const char *s) {
  char *esc = js_escape_sq_dup(s ? s : "");
  sb_addf(sb, "'%s'", esc ? esc : "");
  free(esc);
}

/* A literal for an attr value: number/bool bare, everything else quoted. */
static void emit_literal(Sb *sb, const char *val) {
  if (!val) {
    sb_add(sb, "null");
    return;
  }
  if (cord_looks_like_number(val) || cord_looks_like_bool(val)) {
    sb_add(sb, val);
    return;
  }
  emit_sq_string(sb, val);
}

/*
 * Attribute / child value.
 *   #{…}                              → template literal
 *   number / bool                     → bare literal
 *   identifier declared in this scope → expression
 *   anything else                     → string
 * The scope check is the important part; see the comment on EsmCtx.scope.
 */
static void emit_value(Sb *sb, const char *val, int prefer_string, EsmCtx *c) {
  if (!val) {
    sb_add(sb, "null");
    return;
  }
  if (interp_has(val)) {
    char *body = interp_to_js_template_body(val);
    sb_addf(sb, "`%s`", body ? body : "");
    free(body);
    return;
  }
  if (!prefer_string) {
    if (cord_looks_like_number(val) || cord_looks_like_bool(val)) {
      sb_add(sb, val);
      return;
    }
    if (is_safe_js_path(val) && scope_has(c, val)) {
      sb_add(sb, val);
      return;
    }
  }
  char *plain = interp_plain_text(val);
  emit_sq_string(sb, plain ? plain : val);
  free(plain);
}

/* Handlers: `setCount(count+1)` is a statement, `onSave` is a reference. */
static int handler_needs_arrow(const char *h) {
  if (!h || !*h) return 0;
  return strpbrk(h, "()+-*/ ?:=") != NULL;
}

static void emit_handler(Sb *sb, const char *event, const char *handler) {
  if (!handler) {
    sb_add(sb, "undefined");
    return;
  }
  int is_submit = event && strcmp(event, "submit") == 0;
  if (is_submit) {
    if (handler_needs_arrow(handler))
      sb_addf(sb, "(e) => { e.preventDefault(); %s; }", handler);
    else
      sb_addf(sb, "(e) => { e.preventDefault(); %s(e); }", handler);
    return;
  }
  if (handler_needs_arrow(handler))
    sb_addf(sb, "(e) => { %s; }", handler);
  else
    sb_add(sb, handler);
}

static void event_prop_name(const char *event, char *out, size_t n) {
  if (!event || !*event) {
    snprintf(out, n, "onEvent");
    return;
  }
  snprintf(out, n, "on%c%s", (char)toupper((unsigned char)event[0]), event + 1);
}

/* ── module imports ─────────────────────────────────────── */

static void path_basename_noext(const char *mod_path, char *out, size_t n) {
  const char *base = mod_path;
  for (const char *p = mod_path; *p; p++)
    if (*p == '/' || *p == '\\') base = p + 1;
  snprintf(out, n, "%s", base);
  size_t l = strlen(out);
  if (l > 5 && strcmp(out + l - 5, ".cord") == 0) out[l - 5] = '\0';
}

/* Absolute resolved file → "/src/pages/HomePage.cord" URL under the root. */
static int url_for_resolved(const char *resolved, const char *project_root,
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

static EsmImport *import_find_by_name(EsmCtx *c, const char *name) {
  if (!name) return NULL;
  for (int i = 0; i < c->n_imports; i++)
    if (strcmp(c->imports[i].export_name, name) == 0) return &c->imports[i];
  return NULL;
}

/*
 * Register an import for a module ref. Returns the local export name to use,
 * or NULL when the module cannot be resolved (caller degrades gracefully).
 */
static const char *import_unresolved(EsmCtx *c, const char *mod_path,
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

static const char *import_add(EsmCtx *c, const char *mod_path, const char *alias) {
  if (!mod_path || !*mod_path) return NULL;

  char *resolved = compiler_resolve_module(c->mod->abs_path, mod_path,
                                          c->mod->project_root);
  if (!resolved) return import_unresolved(c, mod_path, alias);

  char url[512];
  url_for_resolved(resolved, c->mod->project_root, url, sizeof(url));
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
  path_basename_noext(mod_path, im->basename, sizeof(im->basename));
  free(ename);
  return im->export_name;
}

/* Package string for a foreign comment: default module, else first backend attr. */
static const char *foreign_mod_hint(const IrNode *n) {
  if (!n) return NULL;
  if (n->value && n->value[0]) return n->value;
  for (size_t i = 0; i < n->n_kids; i++) {
    IrNode *a = n->kids[i];
    if (a && a->kind == IR_ATTR && a->value && a->value[0]) return a->value;
  }
  return NULL;
}

static void layout_export_name_of(const char *orig, char *out, size_t n) {
  if (!orig || !*orig || strcmp(orig, "default") == 0) {
    snprintf(out, n, "DefaultLayout");
    return;
  }
  if (is_pascal(orig)) {
    snprintf(out, n, "%s", orig);
    return;
  }
  snprintf(out, n, "%c%s", (char)toupper((unsigned char)orig[0]), orig + 1);
}

/*
 * Register a foreign host component as a runtime stub. Skips names already
 * covered by a real import, an unresolved-module stub, or a same-file
 * IR_COMPONENT / IR_LAYOUT export (avoids `const X` + `export const X`).
 */
static void foreign_add(EsmCtx *c, const char *name, const char *mod) {
  if (!name || !*name || !is_safe_js_ident(name)) return;
  if (import_find_by_name(c, name)) return;
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

/* True if a same-file component/layout already owns this JS export name. */
static int same_file_export_owns(IrNode *root, const char *name) {
  if (!root || !name || !*name) return 0;
  for (size_t j = 0; j < root->n_kids; j++) {
    IrNode *s = root->kids[j];
    if (!s) continue;
    if (s->kind == IR_COMPONENT && s->name && strcmp(s->name, name) == 0)
      return 1;
    if (s->kind == IR_LAYOUT) {
      char en[96];
      layout_export_name_of(s->name ? s->name : "default", en, sizeof(en));
      if (strcmp(en, name) == 0) return 1;
    }
  }
  return 0;
}

static void collect_foreigns(EsmCtx *c, IrNode *root) {
  if (!c || !root) return;
  for (size_t i = 0; i < root->n_kids; i++) {
    IrNode *d = root->kids[i];
    if (!d || d->kind != IR_FOREIGN || !d->name) continue;
    if (same_file_export_owns(root, d->name)) continue;
    foreign_add(c, d->name, foreign_mod_hint(d));
  }
}

/* ── element / children codegen ─────────────────────────── */

static void gen_node(Sb *sb, IrNode *n, int depth, EsmCtx *c);
static void gen_children_array(Sb *sb, IrNode *parent, int depth, EsmCtx *c);

static int ir_hook_is(const IrNode *n, const char *kind) {
  return n && n->kind == IR_HOOK && n->name && kind &&
         strcmp(n->name, kind) == 0;
}

static int is_decl_node(const IrNode *n) {
  if (!n) return 0;
  switch (n->kind) {
    case IR_PROP:
    case IR_STATE:
    case IR_COMPUTED:
    case IR_EFFECT:
    case IR_FETCH:
    case IR_MODULE_USE:
    case IR_STORE:
    case IR_COMPONENT:
    case IR_LAYOUT:
    case IR_ROUTE:
    case IR_ATTR:
    case IR_EVENT:
    case IR_FOREIGN:
      return 1;
    case IR_HOOK:
      /* Declaration-ish hooks handled in the prelude, not the tree. */
      return ir_hook_is(n, "ref") || ir_hook_is(n, "context") ||
             ir_hook_is(n, "ctx") || ir_hook_is(n, "params") ||
             ir_hook_is(n, "navigate") || ir_hook_is(n, "theme") ||
             ir_hook_is(n, "id") || ir_hook_is(n, "action") ||
             ir_hook_is(n, "lazy") || ir_hook_is(n, "reducer") ||
             ir_hook_is(n, "callback") || ir_hook_is(n, "memo") ||
             ir_hook_is(n, "store");
    default:
      return 0;
  }
}

/* Renderable child (produces DOM), as opposed to a declaration. */
static int is_renderable(const IrNode *n) {
  if (!n) return 0;
  if (n->kind == IR_TEXT && n->value && strcmp(n->value, "__else__") == 0)
    return 0;
  if (is_decl_node(n)) return 0;
  return 1;
}

static int node_has_renderable_kids(IrNode *n) {
  for (size_t i = 0; i < n->n_kids; i++)
    if (is_renderable(n->kids[i])) return 1;
  return 0;
}

/*
 * Bare identifier attrs double as text content in Cord (`p Hello` parses the
 * word as a valueless attr). Mirror the React backend's exclusion list so the
 * same words render as text rather than attributes.
 */
static int bare_attr_is_content(const char *name, int skip_preset_attrs) {
  if (!name || !*name) return 0;
  if (cord_is_style_bool_name(name) || cord_is_style_attr_name(name)) return 0;
  if (cord_is_dom_a11y_or_data_attr(name)) return 0;
  static const char *never[] = {
      "required", "disabled", "readonly", "checked",  "text",   "email",
      "password", "search",   "number",   "lazy",     "forwardRef",
      "__file__", "style",    "to",       "href",     "src",    "alt",
      "type",     "placeholder", "name",  "value",    "id",     "key",
      "rows",     "layout",   "bind",     "ref",      "action", NULL};
  for (int i = 0; never[i]; i++)
    if (strcmp(name, never[i]) == 0) return 0;
  if (skip_preset_attrs && (strcmp(name, "fade") == 0 || strcmp(name, "data") == 0))
    return 0;
  return 1;
}

/*
 * DOM attributes whose value is a plain string in practice. `type=button` and
 * `id=forma` must NOT become identifier references — mirrors the React
 * backend, which only treats these as expressions when the value is dotted or
 * numeric.
 */
static int attr_is_dom_literal(const char *name) {
  return name && (strcmp(name, "src") == 0 || strcmp(name, "alt") == 0 ||
                  strcmp(name, "href") == 0 || strcmp(name, "placeholder") == 0 ||
                  strcmp(name, "type") == 0 || strcmp(name, "rows") == 0 ||
                  strcmp(name, "name") == 0 || strcmp(name, "value") == 0 ||
                  strcmp(name, "id") == 0 || strcmp(name, "key") == 0 ||
                  strcmp(name, "title") == 0 || strcmp(name, "label") == 0 ||
                  strcmp(name, "for") == 0 || strcmp(name, "target") == 0 ||
                  strcmp(name, "rel") == 0 || strcmp(name, "lang") == 0);
}

/*
 * Component props that are almost always literal text in Cord UIs. Without this
 * `CodeSample title="arbol"` would compile to an identifier reference.
 */
static int prop_is_stringish(const char *name) {
  return name && (strcmp(name, "title") == 0 || strcmp(name, "text") == 0 ||
                  strcmp(name, "label") == 0 || strcmp(name, "placeholder") == 0 ||
                  strcmp(name, "alt") == 0 || strcmp(name, "name") == 0);
}

static void emit_dom_attr_value(Sb *sb, const char *val, EsmCtx *c) {
  if (!val) {
    sb_add(sb, "null");
    return;
  }
  if (interp_has(val)) {
    char *body = interp_to_js_template_body(val);
    sb_addf(sb, "`%s`", body ? body : "");
    free(body);
    return;
  }
  if (is_safe_js_path(val) && strchr(val, '.') && scope_has(c, val)) {
    sb_add(sb, val);
    return;
  }
  if (cord_looks_like_number(val)) {
    sb_add(sb, val);
    return;
  }
  char *plain = interp_plain_text(val);
  emit_sq_string(sb, plain ? plain : val);
  free(plain);
}

static void gen_props_object(Sb *sb, IrNode *node, const char *tag,
                             const char *dom_tag, const char *classes,
                             int is_component, EsmCtx *c) {
  int wrote = 0;
  sb_add(sb, "{");

  if (classes && *classes) {
    sb_add(sb, " class: ");
    emit_sq_string(sb, classes);
    wrote = 1;
  }

  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *a = node->kids[i];
    if (!a || a->kind != IR_ATTR || !a->name) continue;
    const char *k = a->name;
    const char *v = a->value;

    if (strcmp(k, "__file__") == 0 || strcmp(k, "style") == 0) continue;
    if (strcmp(k, "key") == 0) continue; /* emitted by the `for` wrapper */

    if (!is_component) {
      /* Layout attrs already folded into `class`. */
      if (cord_is_style_attr_name(k)) continue;
      if (cord_is_style_bool_name(k) && cord_attr_is_true(v)) continue;
    } else {
      if (cord_is_style_attr_name(k) || cord_is_style_bool_name(k)) continue;
    }

    /* `type=display` is typography, folded into class, not an HTML type. */
    if (strcmp(k, "type") == 0 && v &&
        (strcmp(v, "display") == 0 || strcmp(v, "title") == 0 ||
         strcmp(v, "body") == 0 || strcmp(v, "caption") == 0 ||
         strcmp(v, "code") == 0))
      continue;

    if (cord_attr_is_true(v)) {
      /* Valueless: either a real boolean DOM attr, or text content. */
      if (!is_component && bare_attr_is_content(k, 0)) continue;
      if (is_component && bare_attr_is_content(k, 0)) {
        if (wrote) sb_add(sb, ",");
        sb_addf(sb, " %s: true", is_safe_js_ident(k) ? k : "invalid");
        wrote = 1;
        continue;
      }
      if (strcmp(k, "required") == 0 || strcmp(k, "disabled") == 0 ||
          strcmp(k, "readonly") == 0 || strcmp(k, "checked") == 0) {
        if (wrote) sb_add(sb, ",");
        sb_addf(sb, " %s: true", k);
        wrote = 1;
        continue;
      }
      if (cord_is_dom_a11y_or_data_attr(k)) {
        if (wrote) sb_add(sb, ",");
        sb_addf(sb, " '%s': 'true'", k);
        wrote = 1;
        continue;
      }
      /* input type shorthands: `input text` → type=text */
      if (dom_tag && strcmp(dom_tag, "input") == 0 &&
          (strcmp(k, "text") == 0 || strcmp(k, "email") == 0 ||
           strcmp(k, "password") == 0 || strcmp(k, "search") == 0 ||
           strcmp(k, "number") == 0)) {
        if (wrote) sb_add(sb, ",");
        sb_addf(sb, " type: '%s'", k);
        wrote = 1;
      }
      continue;
    }

    /* `bind=field` → controlled value + setter */
    if (strcmp(k, "bind") == 0 && v) {
      if (wrote) sb_add(sb, ",");
      if (strchr(v, '.')) {
        sb_addf(sb, " value: %s", v);
      } else {
        char setter[128];
        snprintf(setter, sizeof(setter), "set%c%s",
                 (char)toupper((unsigned char)v[0]), v + 1);
        sb_addf(sb, " value: %s, onInput: (e) => %s(e.target.value)", v, setter);
        int has_name = 0;
        for (size_t j = 0; j < node->n_kids; j++) {
          IrNode *o = node->kids[j];
          if (o && o->kind == IR_ATTR && o->name && strcmp(o->name, "name") == 0)
            has_name = 1;
        }
        if (!has_name) sb_addf(sb, ", name: '%s'", v);
      }
      wrote = 1;
      continue;
    }

    if (strcmp(k, "ref") == 0 && v) {
      if (wrote) sb_add(sb, ",");
      sb_addf(sb, " ref: %s", v);
      wrote = 1;
      continue;
    }

    /* `to=/guia` on a link → href, so the delegated router picks it up. */
    if (strcmp(k, "to") == 0) {
      const char *href = v ? v : "/";
      if (!interp_has(href) && !url_href_is_safe(href)) href = "#";
      if (wrote) sb_add(sb, ",");
      sb_add(sb, " href: ");
      emit_value(sb, href, 1, c);
      wrote = 1;
      continue;
    }

    if (wrote) sb_add(sb, ",");
    if (is_safe_js_ident(k))
      sb_addf(sb, " %s: ", k);
    else
      sb_addf(sb, " '%s': ", k);

    if (cord_is_dom_a11y_or_data_attr(k)) {
      /* aria/role/data must be DOM strings, never booleans. */
      char *esc = js_escape_sq_dup(v ? v : "");
      sb_addf(sb, "'%s'", esc ? esc : "");
      free(esc);
    } else if (!is_component && attr_is_dom_literal(k)) {
      emit_dom_attr_value(sb, v, c);
    } else {
      emit_value(sb, v, is_component && prop_is_stringish(k), c);
    }
    wrote = 1;
  }

  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *e = node->kids[i];
    if (!e || e->kind != IR_EVENT || !e->name) continue;
    char pname[64];
    event_prop_name(e->name, pname, sizeof(pname));
    if (wrote) sb_add(sb, ",");
    sb_addf(sb, " %s: ", pname);
    emit_handler(sb, e->name, e->value);
    wrote = 1;
  }

  sb_add(sb, wrote ? " }" : "}");
}

static void gen_element(Sb *sb, IrNode *node, int depth, EsmCtx *c) {
  const char *tag = node->name ? node->name : "div";

  /* Component reference: PascalCase tag, imported or locally defined. */
  if (is_pascal(tag)) {
    sb_pad(sb, depth);
    sb_addf(sb, "h(%s, ", tag);
    gen_props_object(sb, node, tag, NULL, NULL, 1, c);
    if (node_has_renderable_kids(node)) {
      sb_add(sb, ", [\n");
      gen_children_array(sb, node, depth + 1, c);
      sb_pad(sb, depth);
      sb_add(sb, "])");
    } else {
      sb_add(sb, ")");
    }
    return;
  }

  if (node->kind == IR_SLOT || strcmp(tag, "slot") == 0) {
    sb_pad(sb, depth);
    sb_add(sb, "props.children");
    c->uses_children = 1;
    return;
  }

  const char *dom_tag = cord_html_tag_for(tag);
  if (!dom_tag) {
    /* `fragment` → children only */
    sb_pad(sb, depth);
    sb_add(sb, "frag([\n");
    gen_children_array(sb, node, depth + 1, c);
    sb_pad(sb, depth);
    sb_add(sb, "])");
    return;
  }

  /*
   * Preset tags (icon / motion / chart) have no npm component here. Degrade to
   * a semantic element carrying the same class hooks rather than emitting an
   * import that cannot resolve.
   */
  int preset_tag = 0;
  if (strcmp(dom_tag, "CordIcon") == 0) {
    dom_tag = "span";
    preset_tag = 1;
  } else if (strcmp(dom_tag, "CordMotion") == 0) {
    dom_tag = "div";
    preset_tag = 1;
  } else if (strcmp(dom_tag, "CordChart") == 0) {
    dom_tag = "div";
    preset_tag = 1;
  }

  const char *base_class = cord_tag_base_class(tag);
  char classes[2048];
  cord_collect_classes(classes, sizeof(classes), node, base_class);
  if (preset_tag) {
    char extra[2048];
    snprintf(extra, sizeof(extra), "cord-%s%s%s", tag, classes[0] ? " " : "",
             classes);
    snprintf(classes, sizeof(classes), "%s", extra);
  }
  const char *cls = classes;
  while (*cls == ' ') cls++;

  sb_pad(sb, depth);
  sb_addf(sb, "h('%s', ", dom_tag);
  gen_props_object(sb, node, tag, dom_tag, cls, 0, c);

  /* checkbox / radio carry an implicit input type */
  if (strcmp(tag, "checkbox") == 0 || strcmp(tag, "radio") == 0) {
    /* rewritten inline: props object already closed, so patch via Object.assign
       would be noisy — emit type through a second props entry instead. */
  }

  int self_closing = (strcmp(dom_tag, "img") == 0 || strcmp(dom_tag, "input") == 0 ||
                      strcmp(dom_tag, "br") == 0 || strcmp(dom_tag, "hr") == 0);

  int has_kids = node_has_renderable_kids(node);
  int has_bare_content = 0;
  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *a = node->kids[i];
    if (a && a->kind == IR_ATTR && a->name && cord_attr_is_true(a->value) &&
        bare_attr_is_content(a->name, preset_tag))
      has_bare_content = 1;
  }

  if (self_closing || (!has_kids && !has_bare_content)) {
    sb_add(sb, ")");
    return;
  }

  sb_add(sb, ", [\n");
  /* Bare identifiers as text content, before real children (React order). */
  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *a = node->kids[i];
    if (!a || a->kind != IR_ATTR || !a->name) continue;
    if (!cord_attr_is_true(a->value)) continue;
    if (!bare_attr_is_content(a->name, preset_tag)) continue;
    sb_pad(sb, depth + 1);
    sb_addf(sb, "%s,\n", a->name);
  }
  gen_children_array(sb, node, depth + 1, c);
  sb_pad(sb, depth);
  sb_add(sb, "])");
}

static void gen_text(Sb *sb, const char *value, int depth) {
  sb_pad(sb, depth);
  if (interp_has(value)) {
    char *body = interp_to_js_template_body(value);
    sb_addf(sb, "`%s`", body ? body : "");
    free(body);
    return;
  }
  char *plain = interp_plain_text(value);
  char *esc = js_escape_sq_dup(plain ? plain : (value ? value : ""));
  sb_addf(sb, "'%s'", esc ? esc : "");
  free(esc);
  free(plain);
}

static void gen_interp(Sb *sb, IrNode *n, int depth) {
  sb_pad(sb, depth);
  if (n->value && n->n_kids == 0) {
    sb_addf(sb, "(%s)", n->value);
    return;
  }
  sb_add(sb, "`");
  for (size_t i = 0; i < n->n_kids; i++) {
    IrNode *k = n->kids[i];
    if (!k) continue;
    if (k->kind == IR_TEXT && k->value) {
      for (const char *p = k->value; *p; p++) {
        if (*p == '`' || *p == '\\') {
          char esc[3] = {'\\', *p, 0};
          sb_add(sb, esc);
        } else if (*p == '$' && p[1] == '{') {
          sb_add(sb, "\\$");
        } else {
          char ch[2] = {*p, 0};
          sb_add(sb, ch);
        }
      }
    } else if (k->kind == IR_INTERP && k->value) {
      sb_addf(sb, "${%s}", k->value);
    }
  }
  sb_add(sb, "`");
}

/* `for item in items` → (items || []).map((item, idx) => …) */
static void gen_for(Sb *sb, IrNode *n, int depth, EsmCtx *c) {
  const char *var = n->name ? n->name : "item";
  const char *list = n->value ? n->value : "items";
  const char *key_expr = NULL;
  for (size_t i = 0; i < n->n_kids; i++) {
    IrNode *a = n->kids[i];
    if (a && a->kind == IR_ATTR && a->name && strcmp(a->name, "key") == 0 &&
        a->value) {
      key_expr = a->value;
      break;
    }
  }
  sb_pad(sb, depth);
  sb_addf(sb, "(%s || []).map((%s, idx) => keyed(", list, var);
  if (key_expr)
    sb_addf(sb, "%s, ", key_expr);
  else
    sb_add(sb, "idx, ");
  int multi = 0;
  int count = 0;
  for (size_t i = 0; i < n->n_kids; i++)
    if (is_renderable(n->kids[i])) count++;
  multi = count != 1;
  if (multi) sb_add(sb, "frag([");
  sb_add(sb, "\n");
  /* The loop variable is only in scope inside the body. */
  int saved_scope = c->n_scope;
  scope_add(c, var);
  scope_add(c, "idx");
  gen_children_array(sb, n, depth + 1, c);
  c->n_scope = saved_scope;
  sb_pad(sb, depth);
  if (multi) sb_add(sb, "])");
  sb_add(sb, "))");
}

/*
 * `if cond` / `else`. The AST puts the else branch in a sibling TEXT marker
 * named __else__, so the caller tells us where it is.
 */
static void gen_if(Sb *sb, IrNode *n, IrNode *else_node, int depth, EsmCtx *c) {
  const char *cond = n->value ? n->value : "true";
  sb_pad(sb, depth);
  sb_addf(sb, "((%s) ? frag([\n", cond);
  gen_children_array(sb, n, depth + 1, c);
  sb_pad(sb, depth);
  sb_add(sb, "]) : ");
  if (else_node) {
    sb_add(sb, "frag([\n");
    gen_children_array(sb, else_node, depth + 1, c);
    sb_pad(sb, depth);
    sb_add(sb, "]))");
  } else {
    sb_add(sb, "null)");
  }
}

static void gen_children_array(Sb *sb, IrNode *parent, int depth, EsmCtx *c) {
  for (size_t i = 0; i < parent->n_kids; i++) {
    IrNode *k = parent->kids[i];
    if (!is_renderable(k)) continue;

    if (k->kind == IR_IF) {
      IrNode *else_node = NULL;
      for (size_t j = i + 1; j < parent->n_kids; j++) {
        IrNode *s = parent->kids[j];
        if (s && s->kind == IR_TEXT && s->value &&
            strcmp(s->value, "__else__") == 0) {
          else_node = s;
          i = j;
          break;
        }
        if (is_renderable(s)) break;
      }
      gen_if(sb, k, else_node, depth, c);
      sb_add(sb, ",\n");
      continue;
    }
    gen_node(sb, k, depth, c);
    sb_add(sb, ",\n");
  }
}

static void gen_node(Sb *sb, IrNode *n, int depth, EsmCtx *c) {
  if (!n) return;
  switch (n->kind) {
    case IR_ELEMENT:
      gen_element(sb, n, depth, c);
      break;
    case IR_SLOT:
      sb_pad(sb, depth);
      sb_add(sb, "props.children");
      c->uses_children = 1;
      break;
    case IR_TEXT:
      gen_text(sb, n->value, depth);
      break;
    case IR_INTERP:
      gen_interp(sb, n, depth);
      break;
    case IR_FOR:
      gen_for(sb, n, depth, c);
      break;
    case IR_IF:
      gen_if(sb, n, NULL, depth, c);
      break;
    case IR_HOOK:
      /* Structural hooks we can express without a framework. */
      if (ir_hook_is(n, "empty")) {
        const char *cond = n->value ? n->value : "true";
        sb_pad(sb, depth);
        sb_addf(sb, "((%s) ? frag([\n", cond);
        gen_children_array(sb, n, depth + 1, c);
        sb_pad(sb, depth);
        sb_add(sb, "]) : null)");
      } else if (ir_hook_is(n, "suspense") || ir_hook_is(n, "loading") ||
                 ir_hook_is(n, "errorBoundary") || ir_hook_is(n, "portal")) {
        /* No async boundary in the preview runtime: render the content. */
        sb_pad(sb, depth);
        sb_add(sb, "frag([\n");
        for (size_t i = 0; i < n->n_kids; i++) {
          IrNode *k = n->kids[i];
          if (!is_renderable(k)) continue;
          if (k->kind == IR_ELEMENT && k->name &&
              strcmp(k->name, "__fallback__") == 0)
            continue;
          gen_node(sb, k, depth + 1, c);
          sb_add(sb, ",\n");
        }
        sb_pad(sb, depth);
        sb_add(sb, "])");
      } else {
        sb_pad(sb, depth);
        sb_add(sb, "null");
      }
      break;
    case IR_FOREIGN:
      /* Declarations only; stubs are emitted once in gen_imports. */
      sb_pad(sb, depth);
      sb_add(sb, "null");
      break;
    default:
      sb_pad(sb, depth);
      sb_add(sb, "frag([\n");
      gen_children_array(sb, n, depth + 1, c);
      sb_pad(sb, depth);
      sb_add(sb, "])");
      break;
  }
}

/* ── component function ─────────────────────────────────── */

static void gen_prelude(Sb *sb, IrNode *def, EsmCtx *c) {
  /* props destructuring with defaults */
  IrNode *props_decl = NULL;
  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *k = def->kids[i];
    if (k->kind == IR_PROP && (!k->name || strcmp(k->name, "__props__") == 0)) {
      props_decl = k;
      break;
    }
  }
  if (props_decl && props_decl->n_kids) {
    sb_add(sb, "  const {");
    int n = 0;
    for (size_t i = 0; i < props_decl->n_kids; i++) {
      IrNode *p = props_decl->kids[i];
      if (!p->name || !is_safe_js_ident(p->name)) continue;
      if (n++) sb_add(sb, ",");
      sb_addf(sb, " %s", p->name);
      scope_add(c, p->name);
      if (p->value) {
        sb_add(sb, " = ");
        emit_literal(sb, p->value);
      }
    }
    sb_add(sb, " } = props;\n");
  }

  /* route params */
  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *k = def->kids[i];
    if (!ir_hook_is(k, "params")) continue;
    const char *names = k->value ? k->value : "id";
    sb_addf(sb, "  const { %s } = $.params();\n", names);
    scope_add_list(c, names);
  }

  /* state */
  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *k = def->kids[i];
    if (k->kind != IR_STATE) continue;
    if (k->name && strcmp(k->name, "__states__") == 0) {
      for (size_t j = 0; j < k->n_kids; j++) {
        IrNode *st = k->kids[j];
        if (!st->name || !is_safe_js_ident(st->name)) continue;
        sb_addf(sb, "  const [%s, set%c%s] = $.state('%s', ", st->name,
                (char)toupper((unsigned char)st->name[0]), st->name + 1,
                st->name);
        emit_literal(sb, st->value ? st->value : "null");
        sb_add(sb, ");\n");
        scope_add(c, st->name);
      }
    } else if (k->name && is_safe_js_ident(k->name)) {
      sb_addf(sb, "  const [%s, set%c%s] = $.state('%s', ", k->name,
              (char)toupper((unsigned char)k->name[0]), k->name + 1, k->name);
      emit_literal(sb, k->value ? k->value : "null");
      sb_add(sb, ");\n");
      scope_add(c, k->name);
    }
  }

  /* refs */
  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *k = def->kids[i];
    if (!ir_hook_is(k, "ref") || !k->value || !is_safe_js_ident(k->value))
      continue;
    sb_addf(sb, "  const %s = $.ref(", k->value);
    if (k->value2) emit_literal(sb, k->value2);
    sb_add(sb, ");\n");
    scope_add(c, k->value);
  }

  /* navigate */
  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *k = def->kids[i];
    if (!ir_hook_is(k, "navigate")) continue;
    const char *nm = k->value && is_safe_js_ident(k->value) ? k->value : "navigate";
    sb_addf(sb, "  const %s = $.navigate;\n", nm);
    scope_add(c, nm);
  }

  /* fetch → resource; expose data under the declared name */
  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *k = def->kids[i];
    if (k->kind != IR_FETCH || !k->name || !is_safe_js_ident(k->name)) continue;
    sb_addf(sb, "  const %s$res = $.resource('%s', ", k->name, k->name);
    emit_value(sb, k->value, 0, c);
    sb_add(sb, ");\n");
    sb_addf(sb, "  const %s = %s$res.data;\n", k->name, k->name);
    sb_addf(sb, "  const %sLoading = %s$res.loading;\n", k->name, k->name);
    sb_addf(sb, "  const %sError = %s$res.error;\n", k->name, k->name);
    scope_add(c, k->name);
    {
      char buf[96];
      snprintf(buf, sizeof(buf), "%sLoading", k->name);
      scope_add(c, buf);
      snprintf(buf, sizeof(buf), "%sError", k->name);
      scope_add(c, buf);
    }
  }

  /*
   * computed: the whole body re-runs on every render, so a plain const is both
   * simpler and fresher than a memo with empty deps.
   */
  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *k = def->kids[i];
    if (k->kind != IR_COMPUTED || !k->name || !is_safe_js_ident(k->name)) continue;
    sb_addf(sb, "  const %s = (%s);\n", k->name, k->value ? k->value : "null");
    scope_add(c, k->name);
  }

  /* effects */
  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *k = def->kids[i];
    if (k->kind != IR_EFFECT) continue;
    const char *deps = k->value ? k->value : "";
    const char *body = k->value2 ? k->value2 : "";
    const char *cleanup = NULL;
    for (size_t j = 0; j < k->n_kids; j++) {
      IrNode *a = k->kids[j];
      if (a && a->kind == IR_ATTR && a->name && strcmp(a->name, "cleanup") == 0)
        cleanup = a->value;
    }
    sb_add(sb, "  $.effect(() => {\n");
    if (body && *body) sb_addf(sb, "    %s;\n", body);
    if (cleanup && *cleanup) {
      sb_add(sb, "    return () => {\n");
      sb_addf(sb, "      %s;\n", cleanup);
      sb_add(sb, "    };\n");
    }
    sb_addf(sb, "  }, [%s]);\n", deps);
  }
}

static void gen_component(Sb *sb, IrNode *def, const char *name, int is_layout,
                          EsmCtx *c) {
  c->is_layout = is_layout;
  c->uses_children = 0;
  c->n_scope = 0; /* scope is per component, imports are per module */

  sb_addf(sb, "export const %s = component('%s', function (props, $) {\n", name,
          name);
  gen_prelude(sb, def, c);

  int count = 0;
  for (size_t i = 0; i < def->n_kids; i++)
    if (is_renderable(def->kids[i])) count++;

  sb_add(sb, "  return ");
  if (count == 1) {
    /* Single root: emit it directly, no fragment wrapper. */
    for (size_t i = 0; i < def->n_kids; i++) {
      IrNode *k = def->kids[i];
      if (!is_renderable(k)) continue;
      if (k->kind == IR_IF) {
        IrNode *else_node = NULL;
        for (size_t j = i + 1; j < def->n_kids; j++) {
          IrNode *s = def->kids[j];
          if (s && s->kind == IR_TEXT && s->value &&
              strcmp(s->value, "__else__") == 0) {
            else_node = s;
            break;
          }
        }
        Sb tmp;
        sb_init(&tmp);
        gen_if(&tmp, k, else_node, 1, c);
        sb_add(sb, tmp.buf ? tmp.buf + 2 : "null");
        free(tmp.buf);
      } else {
        Sb tmp;
        sb_init(&tmp);
        gen_node(&tmp, k, 1, c);
        /* strip the leading indent produced by gen_node */
        const char *p = tmp.buf ? tmp.buf : "";
        while (*p == ' ') p++;
        sb_add(sb, p);
        free(tmp.buf);
      }
      break;
    }
    sb_add(sb, ";\n");
  } else if (count == 0) {
    sb_add(sb, "null;\n");
  } else {
    sb_add(sb, "frag([\n");
    gen_children_array(sb, def, 2, c);
    sb_add(sb, "  ]);\n");
  }
  sb_add(sb, "});\n\n");
}

/* ── module assembly ────────────────────────────────────── */

static IrNode *find_theme(IrNode *root) {
  for (size_t i = 0; i < root->n_kids; i++)
    if (ir_hook_is(root->kids[i], "theme")) return root->kids[i];
  return NULL;
}

static void gen_theme_export(Sb *sb, IrNode *root) {
  IrNode *theme = find_theme(root);
  if (!theme) {
    sb_add(sb, "export const theme = null;\n\n");
    return;
  }
  sb_add(sb, "export const theme = {");
  int n = 0;
  for (size_t i = 0; i < theme->n_kids; i++) {
    IrNode *a = theme->kids[i];
    if (!a || a->kind != IR_ATTR || !a->name) continue;
    if (n++) sb_add(sb, ",");
    sb_addf(sb, " '%s': ", a->name);
    emit_literal(sb, a->value);
  }
  sb_add(sb, n ? " };\n\n" : "};\n\n");
}

/*
 * Routes. Each `route /x => pages/X` becomes an import plus an entry; the
 * layout follows the React backend's rule (explicit `layout=`, else the layout
 * named `default`, else the first one declared).
 */
static void gen_routes(Sb *sb, IrNode *root, EsmCtx *c) {
  /* Register `use` imports first so layouts are known. */
  char first_layout[96];
  char default_layout[96];
  first_layout[0] = '\0';
  default_layout[0] = '\0';

  for (size_t i = 0; i < root->n_kids; i++) {
    IrNode *u = root->kids[i];
    if (!u || u->kind != IR_MODULE_USE) continue;
    const char *path = u->value;
    const char *alias = u->name;
    const char *ename = import_add(c, path, alias);
    if (!ename) continue;
    char base[96];
    path_basename_noext(path ? path : "", base, sizeof(base));
    int looks_layout = (path && (strstr(path, "layout") || strstr(path, "Layout")));
    if (looks_layout) {
      if (!first_layout[0]) snprintf(first_layout, sizeof(first_layout), "%s", ename);
      if (strcmp(base, "default") == 0)
        snprintf(default_layout, sizeof(default_layout), "%s", ename);
    }
  }

  int n_routes = 0;
  for (size_t i = 0; i < root->n_kids; i++)
    if (root->kids[i] && root->kids[i]->kind == IR_ROUTE) n_routes++;

  if (!n_routes) {
    sb_add(sb, "export const routes = [];\n\n");
    return;
  }

  /*
   * IR_ROUTE carries the URL in `name` and the target module/component in
   * `value` (see ir.c). Resolve targets into imports before emitting the array.
   */
  for (size_t i = 0; i < root->n_kids; i++) {
    IrNode *r = root->kids[i];
    if (!r || r->kind != IR_ROUTE || !r->value) continue;
    if (compiler_is_module_ref(r->value)) import_add(c, r->value, NULL);
  }

  sb_add(sb, "export const routes = [\n");
  for (size_t i = 0; i < root->n_kids; i++) {
    IrNode *r = root->kids[i];
    if (!r || r->kind != IR_ROUTE) continue;

    const char *path = r->name ? r->name : "/";
    char comp[128];
    comp[0] = '\0';
    if (r->value) {
      if (compiler_is_module_ref(r->value)) {
        char *ename = compiler_module_export_name(r->value, NULL);
        if (ename) {
          snprintf(comp, sizeof(comp), "%s", ename);
          free(ename);
        }
      } else {
        snprintf(comp, sizeof(comp), "%s", r->value);
      }
    }
    if (!comp[0]) continue;

    /* layout selection */
    const char *layout_attr = NULL;
    for (size_t j = 0; j < r->n_kids; j++) {
      IrNode *a = r->kids[j];
      if (a && a->kind == IR_ATTR && a->name && strcmp(a->name, "layout") == 0 &&
          a->value && a->value[0])
        layout_attr = a->value;
    }
    char layout[96];
    layout[0] = '\0';
    if (layout_attr) {
      char want[96];
      layout_export_name_of(layout_attr, want, sizeof(want));
      if (import_find_by_name(c, want))
        snprintf(layout, sizeof(layout), "%s", want);
      else {
        for (int k = 0; k < c->n_imports; k++) {
          if (strcmp(c->imports[k].basename, layout_attr) == 0) {
            snprintf(layout, sizeof(layout), "%s", c->imports[k].export_name);
            break;
          }
        }
      }
    } else if (default_layout[0]) {
      snprintf(layout, sizeof(layout), "%s", default_layout);
    } else if (first_layout[0]) {
      snprintf(layout, sizeof(layout), "%s", first_layout);
    }

    sb_add(sb, "  { path: ");
    emit_sq_string(sb, path);
    sb_addf(sb, ", component: %s", comp);
    if (layout[0]) sb_addf(sb, ", layout: %s", layout);
    sb_add(sb, " },\n");
  }
  sb_add(sb, "];\n\n");
}

static void gen_imports(Sb *sb, EsmCtx *c) {
  sb_add(sb, "import { h, frag, txt, keyed, component } from '/@cord/runtime.js';\n");
  for (int i = 0; i < c->n_imports; i++)
    sb_addf(sb, "import %s from '%s';\n", c->imports[i].export_name,
            c->imports[i].url);
  sb_add(sb, "\n");
  for (int i = 0; i < c->n_unresolved; i++) {
    char *esc = js_escape_sq_dup(c->unresolved[i].mod_path);
    sb_addf(sb,
            "/* cordlang: modulo no encontrado: %s */\n"
            "const %s = component('%s', function () {\n"
            "  return h('div', { class: 'cord-runtime-error' },\n"
            "           'cordlang: no existe el modulo \\'%s\\'');\n"
            "});\n",
            esc ? esc : "?", c->unresolved[i].export_name,
            c->unresolved[i].export_name, esc ? esc : "?");
    free(esc);
  }
  if (c->n_unresolved) sb_add(sb, "\n");
  for (int i = 0; i < c->n_foreigns; i++) {
    /* A real import for the same name wins (no double-define). */
    if (import_find_by_name(c, c->foreigns[i].name)) continue;
    char *esc_name = js_escape_sq_dup(c->foreigns[i].name);
    const char *mod = c->foreigns[i].mod_path;
    if (mod[0]) {
      char *esc_mod = js_escape_sq_dup(mod);
      sb_addf(sb,
              "/* cordlang: foreign no disponible en preview ESM: %s (%s) */\n"
              "const %s = component('%s', function () {\n"
              "  return h('div', { class: 'cord-runtime-error' },\n"
              "           'cordlang: foreign \\'%s\\' no disponible en preview "
              "ESM');\n"
              "});\n",
              esc_name ? esc_name : "?", esc_mod ? esc_mod : "?",
              c->foreigns[i].name, c->foreigns[i].name,
              esc_name ? esc_name : "?");
      free(esc_mod);
    } else {
      sb_addf(sb,
              "/* cordlang: foreign no disponible en preview ESM: %s */\n"
              "const %s = component('%s', function () {\n"
              "  return h('div', { class: 'cord-runtime-error' },\n"
              "           'cordlang: foreign \\'%s\\' no disponible en preview "
              "ESM');\n"
              "});\n",
              esc_name ? esc_name : "?", c->foreigns[i].name,
              c->foreigns[i].name, esc_name ? esc_name : "?");
    }
    free(esc_name);
  }
  if (c->n_foreigns) sb_add(sb, "\n");
}

char *esm_generate_module(IrProgram *ir, const EsmModuleCtx *mod) {
  if (!ir || !ir->root || !mod) return NULL;

  EsmCtx c;
  memset(&c, 0, sizeof(c));
  c.mod = mod;

  IrNode *root = ir->root;

  /* Body is generated first so imports can be collected, then prepended. */
  Sb body;
  sb_init(&body);

  if (mod->kind == ESM_MOD_ENTRY) {
    gen_theme_export(&body, root);
    gen_routes(&body, root, &c);
  } else {
    for (size_t i = 0; i < root->n_kids; i++) {
      IrNode *u = root->kids[i];
      if (!u || u->kind != IR_MODULE_USE) continue;
      import_add(&c, u->value, u->name);
    }
  }

  /* foreign Chart from "pkg" — no npm in preview; stub before components use it. */
  collect_foreigns(&c, root);

  /* Component / layout definitions declared in this file. */
  const char *primary = NULL;
  for (size_t i = 0; i < root->n_kids; i++) {
    IrNode *d = root->kids[i];
    if (!d || (d->kind != IR_COMPONENT && d->kind != IR_LAYOUT)) continue;
    const char *name = d->name ? d->name : "Component";
    int is_layout = (d->kind == IR_LAYOUT);
    char export_name[96];
    if (is_layout)
      layout_export_name_of(name, export_name, sizeof(export_name));
    else
      snprintf(export_name, sizeof(export_name), "%s", name);
    gen_component(&body, d, export_name, is_layout, &c);
    if (mod->export_name && strcmp(export_name, mod->export_name) == 0)
      primary = mod->export_name;
    if (!primary) primary = NULL;
  }

  /* Pick the default export. */
  char default_name[96];
  default_name[0] = '\0';
  if (mod->kind != ESM_MOD_ENTRY) {
    if (mod->export_name) {
      for (size_t i = 0; i < root->n_kids; i++) {
        IrNode *d = root->kids[i];
        if (!d || (d->kind != IR_COMPONENT && d->kind != IR_LAYOUT)) continue;
        char en[96];
        if (d->kind == IR_LAYOUT)
          layout_export_name_of(d->name ? d->name : "default", en, sizeof(en));
        else
          snprintf(en, sizeof(en), "%s", d->name ? d->name : "Component");
        if (strcmp(en, mod->export_name) == 0) {
          snprintf(default_name, sizeof(default_name), "%s", en);
          break;
        }
      }
    }
    if (!default_name[0]) {
      /* Fall back to the first definition in the file. */
      for (size_t i = 0; i < root->n_kids; i++) {
        IrNode *d = root->kids[i];
        if (!d || (d->kind != IR_COMPONENT && d->kind != IR_LAYOUT)) continue;
        if (d->kind == IR_LAYOUT)
          layout_export_name_of(d->name ? d->name : "default", default_name,
                                sizeof(default_name));
        else
          snprintf(default_name, sizeof(default_name), "%s",
                   d->name ? d->name : "Component");
        break;
      }
    }
  }

  Sb out;
  sb_init(&out);
  /* Normalize separators: the header must not differ between Windows and POSIX. */
  {
    char *src = mod->source_rel ? fs_norm_path(mod->source_rel) : NULL;
    sb_addf(&out, "/* cordlang: source=%s */\n", src ? src : "(inline)");
    free(src);
  }
  gen_imports(&out, &c);
  sb_add(&out, body.buf ? body.buf : "");
  free(body.buf);

  if (mod->kind == ESM_MOD_ENTRY) {
    sb_add(&out, "export default { __cord: 'app', routes: routes, theme: theme");
    /* A single-file app (routes-less entry) still needs a root component. */
    if (default_name[0]) sb_addf(&out, ", component: %s", default_name);
    sb_add(&out, " };\n");
  } else if (default_name[0]) {
    sb_addf(&out, "export default %s;\n", default_name);
  } else {
    sb_add(&out,
           "export default component('Empty', function () { return null; });\n");
  }

  if (c.truncated)
    sb_add(&out, "/* cordlang: import limit reached, some modules omitted */\n");

  return out.buf;
}

/* ── BackendPort surface ────────────────────────────────── */

/*
 * `compile --backend esm` gets the flattened project: emit every component into
 * one module so the output is inspectable and testable without a server.
 */
char *esm_generate_from_ir(IrProgram *ir) {
  if (!ir || !ir->root) return NULL;

  EsmModuleCtx mod;
  memset(&mod, 0, sizeof(mod));
  mod.kind = ESM_MOD_ENTRY;
  mod.source_rel = ir->entry_file;
  mod.abs_path = ir->entry_file;
  mod.project_root = NULL;

  EsmCtx c;
  memset(&c, 0, sizeof(c));
  c.mod = &mod;

  Sb out;
  sb_init(&out);
  /*
   * Basename only: `compile --backend esm` receives whatever path the caller
   * typed (relative or absolute), so echoing it whole would make the output
   * depend on the invocation and no snapshot could ever pin it. The dev server
   * has a real project-relative path and uses it (see esm_generate_module).
   */
  {
    char *base = ir->entry_file ? fs_basename(ir->entry_file) : NULL;
    sb_addf(&out, "/* cordlang: source=%s */\n", base ? base : "(inline)");
    free(base);
  }

  /* Flat `compile --backend esm`: still collect foreigns so stubs bind names. */
  collect_foreigns(&c, ir->root);
  gen_imports(&out, &c);

  gen_theme_export(&out, ir->root);

  for (size_t i = 0; i < ir->root->n_kids; i++) {
    IrNode *d = ir->root->kids[i];
    if (!d || (d->kind != IR_COMPONENT && d->kind != IR_LAYOUT)) continue;
    char en[96];
    if (d->kind == IR_LAYOUT)
      layout_export_name_of(d->name ? d->name : "default", en, sizeof(en));
    else
      snprintf(en, sizeof(en), "%s", d->name ? d->name : "Component");
    gen_component(&out, d, en, d->kind == IR_LAYOUT, &c);
  }

  /* Routes reference already-defined local components in this flat mode. */
  int n_routes = 0;
  for (size_t i = 0; i < ir->root->n_kids; i++)
    if (ir->root->kids[i] && ir->root->kids[i]->kind == IR_ROUTE) n_routes++;

  if (n_routes) {
    char first_layout[96];
    char default_layout[96];
    first_layout[0] = '\0';
    default_layout[0] = '\0';
    for (size_t i = 0; i < ir->root->n_kids; i++) {
      IrNode *d = ir->root->kids[i];
      if (!d || d->kind != IR_LAYOUT) continue;
      char en[96];
      layout_export_name_of(d->name ? d->name : "default", en, sizeof(en));
      if (!first_layout[0]) snprintf(first_layout, sizeof(first_layout), "%s", en);
      if (strcmp(en, "DefaultLayout") == 0)
        snprintf(default_layout, sizeof(default_layout), "%s", en);
    }
    sb_add(&out, "export const routes = [\n");
    for (size_t i = 0; i < ir->root->n_kids; i++) {
      IrNode *r = ir->root->kids[i];
      if (!r || r->kind != IR_ROUTE || !r->value) continue;
      const char *layout_attr = NULL;
      for (size_t j = 0; j < r->n_kids; j++) {
        IrNode *a = r->kids[j];
        if (a && a->kind == IR_ATTR && a->name &&
            strcmp(a->name, "layout") == 0 && a->value && a->value[0])
          layout_attr = a->value;
      }
      char layout[96];
      layout[0] = '\0';
      if (layout_attr)
        layout_export_name_of(layout_attr, layout, sizeof(layout));
      else if (default_layout[0])
        snprintf(layout, sizeof(layout), "%s", default_layout);
      else if (first_layout[0])
        snprintf(layout, sizeof(layout), "%s", first_layout);

      sb_add(&out, "  { path: ");
      emit_sq_string(&out, r->name ? r->name : "/");
      sb_addf(&out, ", component: %s", r->value);
      if (layout[0]) sb_addf(&out, ", layout: %s", layout);
      sb_add(&out, " },\n");
    }
    sb_add(&out, "];\n\n");
    sb_add(&out, "export default { __cord: 'app', routes: routes, theme: theme };\n");
  } else {
    const char *root_comp = NULL;
    static char buf[96];
    for (size_t i = 0; i < ir->root->n_kids; i++) {
      IrNode *d = ir->root->kids[i];
      if (!d || d->kind != IR_COMPONENT) continue;
      snprintf(buf, sizeof(buf), "%s", d->name ? d->name : "Component");
      root_comp = buf;
      break;
    }
    if (root_comp)
      sb_addf(&out, "export default %s;\n", root_comp);
    else
      sb_add(&out,
             "export default component('Empty', function () { return null; });\n");
  }

  return out.buf;
}

char *esm_generate(Node *root) {
  if (!root) return NULL;
  IrProgram *ir = ir_from_ast(root, NULL);
  if (!ir) return NULL;
  char *out = esm_generate_from_ir(ir);
  ir_free(ir);
  return out;
}

static const BackendPort esm_port = {
    .name = "esm",
    .extension = ".js",
    .needs_node_check = 0,
    .generate_from_ir = esm_generate_from_ir,
    .scaffold_from_ir = NULL,
    .generate = esm_generate,
    .scaffold = NULL,
    .scaffold_from_ast = NULL,
};

const BackendPort *esm_backend_port(void) { return &esm_port; }
