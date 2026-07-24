/*
 * React IR-2 codegen: pure IrNode walkers (no ir_origin / AST for body emission).
 */
#include "adapters/outbound/backends/react/react_backend.h"
#include "adapters/outbound/backends/source_attr.h"
#include "adapters/outbound/backends/theme_css.h"
#include "adapters/outbound/backends/ir_walk.h"
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

/* ── small helpers ──────────────────────────────────────── */

static int is_pascal_case(const char *s) {
  return s && s[0] && isupper((unsigned char)s[0]);
}

static int looks_like_number(const char *s) {
  if (!s || !*s) return 0;
  const char *p = s;
  if (*p == '-' || *p == '+') p++;
  int digits = 0;
  while (*p) {
    if (isdigit((unsigned char)*p)) digits++;
    else if (*p != '.') return 0;
    p++;
  }
  return digits > 0;
}

static int looks_like_bool(const char *s) {
  return s && (strcmp(s, "true") == 0 || strcmp(s, "false") == 0);
}

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
  if (after_dot) return 0;
  return 1;
}

/* Props that are almost always string literals in Cord UIs (avoid title={theme}). */
static int is_stringish_prop_name(const char *name) {
  return name &&
         (strcmp(name, "title") == 0 || strcmp(name, "text") == 0 ||
          strcmp(name, "label") == 0 || strcmp(name, "placeholder") == 0 ||
          strcmp(name, "alt") == 0 || strcmp(name, "name") == 0);
}

static int needs_js_arrow(const char *handler) {
  if (!handler || !*handler) return 0;
  if (strchr(handler, '(') || strchr(handler, '+') || strchr(handler, '-') ||
      strchr(handler, '*') || strchr(handler, '/') || strchr(handler, ' ') ||
      strchr(handler, '?') || strchr(handler, ':') || strchr(handler, '='))
    return 1;
  return 0;
}

static int attr_is_true(const char *v) {
  return !v || !*v || strcmp(v, "true") == 0;
}

static int is_style_attr_name(const char *name) {
  if (!name) return 0;
  if ((strncmp(name, "sm:", 3) == 0 || strncmp(name, "md:", 3) == 0 ||
       strncmp(name, "lg:", 3) == 0) &&
      name[3])
    return is_style_attr_name(name + 3);
  return (strcmp(name, "variant") == 0 || strcmp(name, "size") == 0 ||
          strcmp(name, "color") == 0 || strcmp(name, "gap") == 0 ||
          strcmp(name, "cols") == 0 || strcmp(name, "p") == 0 ||
          strcmp(name, "bg") == 0 || strcmp(name, "shadow") == 0 ||
          strcmp(name, "rounded") == 0 || strcmp(name, "max-w") == 0 ||
          strcmp(name, "overflow") == 0 || strcmp(name, "fit") == 0 ||
          strcmp(name, "aspect") == 0 || strcmp(name, "lines") == 0 ||
          strcmp(name, "w") == 0 || strcmp(name, "h") == 0 ||
          strcmp(name, "min-h") == 0 || strcmp(name, "border") == 0 ||
          strcmp(name, "class") == 0 ||
          strcmp(name, "mx") == 0 || strcmp(name, "my") == 0 ||
          strcmp(name, "px") == 0 || strcmp(name, "py") == 0 ||
          strcmp(name, "m") == 0 || strcmp(name, "op") == 0 ||
          strcmp(name, "z") == 0 || strcmp(name, "leading") == 0 ||
          strcmp(name, "tracking") == 0 || strcmp(name, "elevate") == 0 ||
          strcmp(name, "density") == 0);
}

static int is_style_bool_name(const char *name) {
  return name &&
         (strcmp(name, "between") == 0 || strcmp(name, "center") == 0 ||
          strcmp(name, "around") == 0 || strcmp(name, "evenly") == 0 ||
          strcmp(name, "bold") == 0 || strcmp(name, "muted") == 0 ||
          strcmp(name, "sticky") == 0 || strcmp(name, "primary") == 0 ||
          strcmp(name, "outline") == 0 || strcmp(name, "ghost") == 0 ||
          strcmp(name, "secondary") == 0 || strcmp(name, "xs") == 0 ||
          strcmp(name, "sm") == 0 || strcmp(name, "lg") == 0 ||
          strcmp(name, "xl") == 0 || strcmp(name, "2xl") == 0 ||
          strcmp(name, "3xl") == 0 || strcmp(name, "4xl") == 0 ||
          strcmp(name, "flex-1") == 0 || strcmp(name, "font-mono") == 0 ||
          strcmp(name, "border") == 0);
}

static const char *tag_to_div_plus_class(const char *tag) {
  if (!tag) return NULL;
  if (strcmp(tag, "col") == 0) return "flex flex-col";
  if (strcmp(tag, "row") == 0) return "flex flex-row";
  if (strcmp(tag, "stack") == 0) return "flex flex-col";
  if (strcmp(tag, "grid") == 0) return "grid";
  if (strcmp(tag, "page") == 0) return "min-h-screen";
  if (strcmp(tag, "section") == 0) return "cord-section";
  if (strcmp(tag, "btn") == 0) return "btn";
  if (strcmp(tag, "card") == 0) return "card";
  return NULL;
}

static const char *html_tag_for(const char *tag) {
  if (!tag) return "div";
  if (strcmp(tag, "col") == 0 || strcmp(tag, "row") == 0 ||
      strcmp(tag, "stack") == 0 || strcmp(tag, "page") == 0 ||
      strcmp(tag, "card") == 0 || strcmp(tag, "grid") == 0 ||
      strcmp(tag, "group") == 0)
    return "div";
  if (strcmp(tag, "btn") == 0 || strcmp(tag, "button") == 0) return "button";
  if (strcmp(tag, "link") == 0) return "a";
  if (strcmp(tag, "fragment") == 0) return NULL;
  if (strcmp(tag, "checkbox") == 0 || strcmp(tag, "radio") == 0) return "input";
  if (strcmp(tag, "icon") == 0) return "CordIcon";
  if (strcmp(tag, "motion") == 0 || strcmp(tag, "Motion") == 0) return "CordMotion";
  if (strcmp(tag, "chart") == 0 || strcmp(tag, "Chart") == 0) return "CordChart";
  return tag;
}

/* ── generation context ─────────────────────────────────── */

typedef struct {
  int use_router;
  int use_state;
  int use_memo;
  int use_effect;
  int use_ref;
  int use_context;
  int use_reducer;
  int use_params;
  int use_navigate;
  int use_callback;
  int use_id;
  int use_transition;
  int use_deferred;
  int use_action;
  int use_lazy;
  int use_suspense;
  int use_layout_effect;
  int use_insertion_effect;
  int use_effect_event;
  int use_sync_external_store;
  int use_imperative_handle;
  int use_forward_ref;
  int use_portal;
  int use_error_boundary;
  int use_children;
  int use_cord_icon;
  int use_cord_motion;
  int use_cord_chart;
  int in_component;
  int is_layout;
  char action_pending[64];
  char action_fn[64];
  char action_handler[64];
  int in_form_with_action;
} GenCtx;

static void gen_ir_node(StrBuf *sb, IrNode *node, int depth, GenCtx *ctx);
static void gen_ir_children(StrBuf *sb, IrNode *node, int depth, GenCtx *ctx);
static void gen_ir_interpolation(StrBuf *sb, IrNode *node, int depth);

static void emit_jsx_value(StrBuf *sb, const char *val, int force_expr) {
  if (!val) {
    sb_append(sb, "{undefined}");
    return;
  }
  if (looks_like_number(val) || looks_like_bool(val) || force_expr) {
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
  char *esc = js_escape_dq_dup(val);
  sb_appendf(sb, "\"%s\"", esc ? esc : "");
  free(esc);
}

static void emit_js_literal(StrBuf *sb, const char *val) {
  if (!val) {
    sb_append(sb, "null");
    return;
  }
  if (looks_like_number(val) || looks_like_bool(val)) {
    sb_append(sb, val);
    return;
  }
  char *esc = js_escape_dq_dup(val);
  sb_appendf(sb, "\"%s\"", esc ? esc : "");
  free(esc);
}

/* Style map entries under IR_ATTR "style" or direct style keys */
static void append_style_entry_class(char *classes, size_t classes_sz,
                                     const char *key, const char *val) {
  char vbuf[96];
  if (!key) return;
  if (!val) val = "";
  if (strcmp(key, "between") == 0)
    strncat(classes, " justify-between", classes_sz - strlen(classes) - 1);
  else if (strcmp(key, "center") == 0)
    strncat(classes, " items-center justify-center",
            classes_sz - strlen(classes) - 1);
  else if (strcmp(key, "around") == 0)
    strncat(classes, " justify-around", classes_sz - strlen(classes) - 1);
  else if (strcmp(key, "evenly") == 0)
    strncat(classes, " justify-evenly", classes_sz - strlen(classes) - 1);
  else if (strcmp(key, "sticky") == 0)
    strncat(classes, " sticky top-0", classes_sz - strlen(classes) - 1);
  else if (strcmp(key, "bold") == 0)
    strncat(classes, " font-bold", classes_sz - strlen(classes) - 1);
  else if (strcmp(key, "muted") == 0)
    strncat(classes, " text-muted", classes_sz - strlen(classes) - 1);
  else if (strcmp(key, "overflow") == 0) {
    snprintf(vbuf, sizeof(vbuf), " overflow-%s", val);
    strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
  } else if (strcmp(key, "bg") == 0 || strcmp(key, "text") == 0 ||
             strcmp(key, "p") == 0 || strcmp(key, "px") == 0 ||
             strcmp(key, "py") == 0 || strcmp(key, "m") == 0 ||
             strcmp(key, "mx") == 0 || strcmp(key, "my") == 0 ||
             strcmp(key, "gap") == 0 || strcmp(key, "rounded") == 0 ||
             strcmp(key, "shadow") == 0 || strcmp(key, "size") == 0 ||
             strcmp(key, "w") == 0 || strcmp(key, "h") == 0 ||
             strcmp(key, "max-w") == 0 || strcmp(key, "z") == 0 ||
             strcmp(key, "op") == 0 || strcmp(key, "leading") == 0 ||
             strcmp(key, "tracking") == 0) {
    const char *prefix = key;
    if (strcmp(key, "size") == 0) prefix = "text";
    if (strcmp(key, "op") == 0) prefix = "opacity";
    snprintf(vbuf, sizeof(vbuf), " %s-%s", prefix, val);
    strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
  }
}

static void collect_classes_ir(char *classes, size_t classes_sz, IrNode *node,
                               const char *base_class) {
  classes[0] = '\0';
  if (base_class) strncat(classes, base_class, classes_sz - 1);
  if (!node) return;

  int has_between = 0;
  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *child = node->kids[i];
    if (!child || child->kind != IR_ATTR || !child->name) continue;
    if (strcmp(child->name, "between") == 0) has_between = 1;
  }

  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *child = node->kids[i];
    if (!child || child->kind != IR_ATTR || !child->name) continue;
    if (strcmp(child->name, "style") == 0) {
      for (size_t j = 0; j < child->n_kids; j++) {
        IrNode *e = child->kids[j];
        if (e && e->kind == IR_ATTR && e->name)
          append_style_entry_class(classes, classes_sz, e->name,
                                   e->value ? e->value : "");
      }
    }
  }

  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *child = node->kids[i];
    char vbuf[96];
    if (!child || child->kind != IR_ATTR || !child->name) continue;
    if (strcmp(child->name, "style") == 0 || strcmp(child->name, "__file__") == 0)
      continue;
    const char *k = child->name;
    const char *v = child->value ? child->value : "";

    if (attr_is_true(v)) {
      if (strcmp(k, "between") == 0)
        strncat(classes,
                strstr(classes, "flex") ? " justify-between"
                                        : " flex justify-between",
                classes_sz - strlen(classes) - 1);
      else if (strcmp(k, "center") == 0)
        strncat(classes,
                has_between ? " flex items-center"
                            : " flex items-center justify-center",
                classes_sz - strlen(classes) - 1);
      else if (strcmp(k, "around") == 0)
        strncat(classes, " justify-around", classes_sz - strlen(classes) - 1);
      else if (strcmp(k, "evenly") == 0)
        strncat(classes, " justify-evenly", classes_sz - strlen(classes) - 1);
      else if (strcmp(k, "bold") == 0)
        strncat(classes, " font-bold", classes_sz - strlen(classes) - 1);
      else if (strcmp(k, "muted") == 0)
        strncat(classes, " text-muted", classes_sz - strlen(classes) - 1);
      else if (strcmp(k, "font-mono") == 0)
        strncat(classes, " font-mono", classes_sz - strlen(classes) - 1);
      else if (strcmp(k, "flex-1") == 0)
        strncat(classes, " flex-1", classes_sz - strlen(classes) - 1);
      else if (strcmp(k, "border") == 0)
        strncat(classes, " border border-gray-200",
                classes_sz - strlen(classes) - 1);
      else if (strcmp(k, "sticky") == 0)
        strncat(classes, " sticky top-0", classes_sz - strlen(classes) - 1);
      else if (strcmp(k, "primary") == 0)
        strncat(classes, " btn-primary", classes_sz - strlen(classes) - 1);
      else if (strcmp(k, "outline") == 0)
        strncat(classes, " btn-outline", classes_sz - strlen(classes) - 1);
      else if (strcmp(k, "ghost") == 0)
        strncat(classes, " btn-ghost", classes_sz - strlen(classes) - 1);
      else if (strcmp(k, "secondary") == 0)
        strncat(classes, " btn-secondary", classes_sz - strlen(classes) - 1);
      else if (strcmp(k, "xs") == 0)
        strncat(classes, " text-xs", classes_sz - strlen(classes) - 1);
      else if (strcmp(k, "sm") == 0)
        strncat(classes, " text-sm", classes_sz - strlen(classes) - 1);
      else if (strcmp(k, "lg") == 0)
        strncat(classes, " text-lg", classes_sz - strlen(classes) - 1);
      else if (strcmp(k, "xl") == 0)
        strncat(classes, " text-xl", classes_sz - strlen(classes) - 1);
      else if (strcmp(k, "2xl") == 0)
        strncat(classes, " text-2xl", classes_sz - strlen(classes) - 1);
      else if (strcmp(k, "3xl") == 0)
        strncat(classes, " text-3xl", classes_sz - strlen(classes) - 1);
      else if (strcmp(k, "4xl") == 0)
        strncat(classes, " text-4xl", classes_sz - strlen(classes) - 1);
      continue;
    }

    if (strcmp(k, "variant") == 0) {
      snprintf(vbuf, sizeof(vbuf), " btn-%s", v);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(k, "size") == 0) {
      snprintf(vbuf, sizeof(vbuf), " text-%s", v);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(k, "color") == 0) {
      if (theme_is_color_token(v)) {
        char tok[64];
        if (theme_token_name(v, tok, sizeof(tok))) {
          snprintf(vbuf, sizeof(vbuf), " text-[var(--color-%s)]", tok);
          strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
        }
      } else {
        snprintf(vbuf, sizeof(vbuf), " text-%s", v);
        strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
      }
    } else if (strcmp(k, "gap") == 0) {
      snprintf(vbuf, sizeof(vbuf), " gap-%s", v);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(k, "cols") == 0) {
      snprintf(vbuf, sizeof(vbuf), " grid-cols-%s", v);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(k, "p") == 0) {
      snprintf(vbuf, sizeof(vbuf), " p-%s", v);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(k, "px") == 0) {
      snprintf(vbuf, sizeof(vbuf), " px-%s", v);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(k, "py") == 0) {
      snprintf(vbuf, sizeof(vbuf), " py-%s", v);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(k, "m") == 0) {
      snprintf(vbuf, sizeof(vbuf), " m-%s", v);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(k, "mx") == 0) {
      snprintf(vbuf, sizeof(vbuf), " mx-%s", v);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(k, "my") == 0) {
      snprintf(vbuf, sizeof(vbuf), " my-%s", v);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(k, "bg") == 0) {
      if (theme_is_color_token(v)) {
        char tok[64];
        if (theme_token_name(v, tok, sizeof(tok))) {
          snprintf(vbuf, sizeof(vbuf), " bg-[var(--color-%s)]", tok);
          strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
        }
      } else {
        snprintf(vbuf, sizeof(vbuf), " bg-%s", v);
        strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
      }
    } else if (strcmp(k, "shadow") == 0) {
      snprintf(vbuf, sizeof(vbuf), " shadow-%s", v);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(k, "rounded") == 0) {
      snprintf(vbuf, sizeof(vbuf), " rounded-%s", v);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(k, "max-w") == 0) {
      snprintf(vbuf, sizeof(vbuf), " max-w-%s", v);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(k, "w") == 0) {
      snprintf(vbuf, sizeof(vbuf), " w-%s", v);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(k, "h") == 0) {
      snprintf(vbuf, sizeof(vbuf), " h-%s", v);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(k, "min-h") == 0) {
      snprintf(vbuf, sizeof(vbuf), " min-h-%s", v);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(k, "border") == 0) {
      snprintf(vbuf, sizeof(vbuf), " border border-%s", v);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(k, "z") == 0) {
      snprintf(vbuf, sizeof(vbuf), " z-%s", v);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(k, "class") == 0 && v && *v) {
      /* Arbitrary utility / site.css classes from Cord `class=...` */
      strncat(classes, " ", classes_sz - strlen(classes) - 1);
      strncat(classes, v, classes_sz - strlen(classes) - 1);
    } else if (strcmp(k, "type") == 0 &&
               (strcmp(v, "display") == 0 || strcmp(v, "title") == 0 ||
                strcmp(v, "body") == 0 || strcmp(v, "caption") == 0 ||
                strcmp(v, "code") == 0)) {
      snprintf(vbuf, sizeof(vbuf), " type-%s", v);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(k, "elevate") == 0) {
      snprintf(vbuf, sizeof(vbuf), " elevate-%s", v);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(k, "density") == 0) {
      snprintf(vbuf, sizeof(vbuf), " density-%s", v);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(k, "leading") == 0) {
      snprintf(vbuf, sizeof(vbuf), " leading-%s", v);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strcmp(k, "tracking") == 0) {
      snprintf(vbuf, sizeof(vbuf), " tracking-%s", v);
      strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
    } else if (strncmp(k, "sm:", 3) == 0 || strncmp(k, "md:", 3) == 0 ||
               strncmp(k, "lg:", 3) == 0) {
      const char *bp = k;
      const char *rest = k + 3;
      char inner[96];
      inner[0] = '\0';
      if (strcmp(rest, "gap") == 0) snprintf(inner, sizeof(inner), "gap-%s", v);
      else if (strcmp(rest, "p") == 0) snprintf(inner, sizeof(inner), "p-%s", v);
      else if (strcmp(rest, "px") == 0) snprintf(inner, sizeof(inner), "px-%s", v);
      else if (strcmp(rest, "py") == 0) snprintf(inner, sizeof(inner), "py-%s", v);
      else if (strcmp(rest, "cols") == 0)
        snprintf(inner, sizeof(inner), "grid-cols-%s", v);
      else if (strcmp(rest, "m") == 0) snprintf(inner, sizeof(inner), "m-%s", v);
      else if (strcmp(rest, "w") == 0) snprintf(inner, sizeof(inner), "w-%s", v);
      if (inner[0]) {
        snprintf(vbuf, sizeof(vbuf), " %.2s:%s", bp, inner);
        strncat(classes, vbuf, classes_sz - strlen(classes) - 1);
      }
    }
  }
}

static int ir_hook_is(const IrNode *n, const char *kind) {
  return n && n->kind == IR_HOOK && n->name && kind && strcmp(n->name, kind) == 0;
}

static int is_decl_ir(const IrNode *n) {
  if (!n) return 0;
  if (n->kind == IR_PROP || n->kind == IR_STATE || n->kind == IR_COMPUTED ||
      n->kind == IR_EFFECT || n->kind == IR_FETCH || n->kind == IR_MODULE_USE)
    return 1;
  if (n->kind == IR_HOOK) {
    /* body-emitting hooks */
    if (ir_hook_is(n, "portal") || ir_hook_is(n, "errorBoundary") ||
        ir_hook_is(n, "suspense") || ir_hook_is(n, "loading") ||
        ir_hook_is(n, "empty"))
      return 0;
    return 1; /* ref, ctx, action, head, lazy, … */
  }
  if (n->kind == IR_ATTR && n->name &&
      (strcmp(n->name, "forwardRef") == 0 || strcmp(n->name, "__file__") == 0))
    return 1;
  return 0;
}

/* ── multi-file project ─────────────────────────────────── */

typedef enum { RK_PAGE = 0, RK_COMPONENT = 1, RK_LAYOUT = 2 } ReactKind;

#define REACT_MAX_UNITS 64
#define REACT_MAX_USED 24
#define REACT_MAX_ROUTES 64

typedef struct {
  char name[96];
  char dir[16];
  char rel[128];
  char source_path[256];
  char layout_src[96]; /* original layout name: default, shop */
  IrNode *ir_def;
  ReactKind kind;
  int use_state;
  int use_memo;
  int use_effect;
  int use_ref;
  int use_context;
  int use_reducer;
  int use_params;
  int use_navigate;
  int use_callback;
  int use_id;
  int use_transition;
  int use_deferred;
  int use_action;
  int use_lazy;
  int use_suspense;
  int use_layout_effect;
  int use_insertion_effect;
  int use_effect_event;
  int use_sync_external_store;
  int use_imperative_handle;
  int use_forward_ref;
  int use_portal;
  int use_error_boundary;
  int use_link;
  int use_outlet;
  int use_children;
  int use_fragment;
  char action_stubs[8][64];
  int n_action_stubs;
  char used[REACT_MAX_USED][96];
  int n_used;
  char ctx_names[16][64];
  int n_ctx;
  char lazy_names[16][64];
  char lazy_paths[16][128];
  int n_lazy;
} ReactUnit;

typedef struct {
  ReactUnit units[REACT_MAX_UNITS];
  int n_units;
  IrNode *routes[REACT_MAX_ROUTES];
  int n_routes;
  IrNode *page_nodes[64];
  int n_page_nodes;
  IrNode *contexts[32]; /* IR_HOOK "context" */
  int n_contexts;
  IrNode *foreigns[32];
  int n_foreigns;
  int has_router;
  char lazy_route_names[16][64];
  int n_lazy_routes;
  int truncated; /* hard limit hit — emit must fail */
} ReactProject;

static int route_has_lazy_attr_ir(IrNode *route) {
  return irw_route_has_lazy(route);
}

static int project_is_lazy_route_name(ReactProject *proj, const char *name) {
  if (!proj || !name) return 0;
  for (int i = 0; i < proj->n_lazy_routes; i++) {
    if (strcmp(proj->lazy_route_names[i], name) == 0) return 1;
  }
  return 0;
}

static int route_is_lazy_ir(ReactProject *proj, IrNode *route) {
  if (!route) return 0;
  if (route_has_lazy_attr_ir(route)) return 1;
  if (route->value && project_is_lazy_route_name(proj, route->value)) return 1;
  return 0;
}

static void layout_export_name(const char *orig, char *out, size_t n) {
  if (!orig || !*orig || strcmp(orig, "default") == 0) {
    snprintf(out, n, "DefaultLayout");
    return;
  }
  if (is_pascal_case(orig)) {
    snprintf(out, n, "%s", orig);
    return;
  }
  snprintf(out, n, "%c%s", (char)toupper((unsigned char)orig[0]), orig + 1);
}

static int name_ends_with(const char *s, const char *suffix) {
  size_t ls = s ? strlen(s) : 0, lf = strlen(suffix);
  return ls >= lf && strcmp(s + ls - lf, suffix) == 0;
}

static int is_route_target_ir(const char *name, IrNode **routes, int n_routes) {
  if (!name) return 0;
  for (int i = 0; i < n_routes; i++) {
    if (routes[i]->value && strcmp(routes[i]->value, name) == 0) return 1;
  }
  return 0;
}

static void unit_add_used(ReactUnit *u, const char *name) {
  if (!name || !is_pascal_case(name)) return;
  if (strcmp(name, u->name) == 0) return;
  for (int i = 0; i < u->n_used; i++) {
    if (strcmp(u->used[i], name) == 0) return;
  }
  if (u->n_used < REACT_MAX_USED) {
    snprintf(u->used[u->n_used], sizeof(u->used[0]), "%s", name);
    u->n_used++;
  }
}

static void scan_tree_deps_ir(IrNode *n, ReactUnit *u) {
  if (!n) return;
  if (n->kind == IR_SLOT) {
    if (u->kind == RK_LAYOUT) u->use_outlet = 1;
    else u->use_children = 1;
  }
  if (n->kind == IR_ELEMENT && n->name) {
    if (is_pascal_case(n->name)) unit_add_used(u, n->name);
    if (strcmp(n->name, "link") == 0) u->use_link = 1;
    if (strcmp(n->name, "provide") == 0) u->use_context = 1;
    if (strcmp(n->name, "slot") == 0) {
      if (u->kind == RK_LAYOUT) u->use_outlet = 1;
      else u->use_children = 1;
    }
  }
  if (n->kind == IR_STATE) u->use_state = 1;
  if (n->kind == IR_COMPUTED) u->use_memo = 1;
  if (n->kind == IR_EFFECT) {
    if (n->name && strcmp(n->name, "layoutEffect") == 0)
      u->use_layout_effect = 1;
    else if (n->name && strcmp(n->name, "insertionEffect") == 0)
      u->use_insertion_effect = 1;
    else
      u->use_effect = 1;
  }
  if (n->kind == IR_FETCH) {
    u->use_state = 1;
    u->use_effect = 1;
  }
  if (n->kind == IR_FOR) u->use_fragment = 1;
  if (n->kind == IR_HOOK && n->name) {
    if (strcmp(n->name, "ref") == 0) u->use_ref = 1;
    else if (strcmp(n->name, "ctx") == 0) u->use_context = 1;
    else if (strcmp(n->name, "context") == 0) {
      u->use_context = 1;
      if (n->value && u->n_ctx < 16) {
        snprintf(u->ctx_names[u->n_ctx], sizeof(u->ctx_names[0]), "%s",
                 n->value);
        u->n_ctx++;
      }
    } else if (strcmp(n->name, "reducer") == 0)
      u->use_reducer = 1;
    else if (strcmp(n->name, "params") == 0)
      u->use_params = 1;
    else if (strcmp(n->name, "navigate") == 0)
      u->use_navigate = 1;
    else if (strcmp(n->name, "callback") == 0)
      u->use_callback = 1;
    else if (strcmp(n->name, "id") == 0)
      u->use_id = 1;
    else if (strcmp(n->name, "transition") == 0)
      u->use_transition = 1;
    else if (strcmp(n->name, "deferred") == 0)
      u->use_deferred = 1;
    else if (strcmp(n->name, "action") == 0) {
      u->use_action = 1;
      if (n->value2 && n->value2[0]) {
        int simple = 1;
        for (const char *p = n->value2; *p; p++) {
          if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
                (*p >= '0' && *p <= '9') || *p == '_' || *p == '$')) {
            simple = 0;
            break;
          }
        }
        if (simple && u->n_action_stubs < 8) {
          int found = 0;
          for (int i = 0; i < u->n_action_stubs; i++) {
            if (strcmp(u->action_stubs[i], n->value2) == 0) {
              found = 1;
              break;
            }
          }
          if (!found) {
            snprintf(u->action_stubs[u->n_action_stubs],
                     sizeof(u->action_stubs[0]), "%s", n->value2);
            u->n_action_stubs++;
          }
        }
      }
    } else if (strcmp(n->name, "lazy") == 0) {
      u->use_lazy = 1;
      if (n->value && n->value2 && u->n_lazy < 16) {
        snprintf(u->lazy_names[u->n_lazy], sizeof(u->lazy_names[0]), "%s",
                 n->value);
        snprintf(u->lazy_paths[u->n_lazy], sizeof(u->lazy_paths[0]), "%s",
                 n->value2);
        u->n_lazy++;
      }
    } else if (strcmp(n->name, "effectEvent") == 0)
      u->use_effect_event = 1;
    else if (strcmp(n->name, "externalStore") == 0)
      u->use_sync_external_store = 1;
    else if (strcmp(n->name, "imperativeHandle") == 0)
      u->use_imperative_handle = 1;
    else if (strcmp(n->name, "head") == 0)
      u->use_effect = 1;
    else if (strcmp(n->name, "portal") == 0)
      u->use_portal = 1;
    else if (strcmp(n->name, "errorBoundary") == 0)
      u->use_error_boundary = 1;
    else if (strcmp(n->name, "suspense") == 0 || strcmp(n->name, "loading") == 0)
      u->use_suspense = 1;
  }
  if (n->kind == IR_ATTR && n->name && strcmp(n->name, "forwardRef") == 0 &&
      attr_is_true(n->value))
    u->use_forward_ref = 1;

  for (size_t i = 0; i < n->n_kids; i++) scan_tree_deps_ir(n->kids[i], u);
}

static ReactUnit *project_find_unit(ReactProject *p, const char *name) {
  if (!name) return NULL;
  for (int i = 0; i < p->n_units; i++) {
    if (strcmp(p->units[i].name, name) == 0) return &p->units[i];
  }
  return NULL;
}

static void project_partition_from_ir(ReactProject *p, IrProgram *ir) {
  memset(p, 0, sizeof(*p));
  if (!ir || !ir->root) return;
  IrNode *root = ir->root;

  IrNode *raw_comps[REACT_MAX_UNITS];
  IrNode *raw_layouts[16];
  int n_comp = 0, n_layout = 0;

  for (size_t i = 0; i < root->n_kids; i++) {
    IrNode *c = root->kids[i];
    if (!c) continue;
    if (c->kind == IR_COMPONENT) {
      if (n_comp < REACT_MAX_UNITS) raw_comps[n_comp++] = c;
      else p->truncated = 1;
    } else if (c->kind == IR_LAYOUT) {
      if (n_layout < 16) raw_layouts[n_layout++] = c;
      else p->truncated = 1;
    } else if (c->kind == IR_ROUTE) {
      if (p->n_routes < REACT_MAX_ROUTES) p->routes[p->n_routes++] = c;
      else p->truncated = 1;
    } else if (ir_hook_is(c, "lazy") && c->value) {
      if (p->n_lazy_routes < 16) {
        snprintf(p->lazy_route_names[p->n_lazy_routes],
                 sizeof(p->lazy_route_names[0]), "%s", c->value);
        p->n_lazy_routes++;
      } else {
        p->truncated = 1;
      }
    } else if (ir_hook_is(c, "context")) {
      if (p->n_contexts < 32) p->contexts[p->n_contexts++] = c;
      else p->truncated = 1;
    } else if (c->kind == IR_FOREIGN) {
      if (p->n_foreigns < 32) p->foreigns[p->n_foreigns++] = c;
      else p->truncated = 1;
    } else if (!ir_hook_is(c, "theme")) {
      if (p->n_page_nodes < 64) p->page_nodes[p->n_page_nodes++] = c;
      else p->truncated = 1;
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
    if (p->n_units >= REACT_MAX_UNITS) {
      p->truncated = 1;
      break;
    }
    ReactUnit *u = &p->units[p->n_units++];
    memset(u, 0, sizeof(*u));
    snprintf(u->name, sizeof(u->name), "%s",
             raw_comps[i]->name ? raw_comps[i]->name : "Component");
    u->ir_def = raw_comps[i];
    if (is_route_target_ir(u->name, p->routes, p->n_routes) ||
        name_ends_with(u->name, "Page")) {
      u->kind = RK_PAGE;
      snprintf(u->dir, sizeof(u->dir), "%s", "pages");
    } else {
      u->kind = RK_COMPONENT;
      snprintf(u->dir, sizeof(u->dir), "%s", "components");
    }
    {
      char relbuf[128];
      snprintf(relbuf, sizeof(relbuf), "%s/%s.jsx", u->dir, u->name);
      snprintf(u->rel, sizeof(u->rel), "%s", relbuf);
    }
    cord_guess_source_path(u->name, u->dir, u->source_path,
                           sizeof(u->source_path));
    scan_tree_deps_ir(u->ir_def, u);
  }

  for (int i = 0; i < n_layout; i++) {
    if (p->n_units >= REACT_MAX_UNITS) {
      p->truncated = 1;
      break;
    }
    ReactUnit *u = &p->units[p->n_units++];
    memset(u, 0, sizeof(*u));
    layout_export_name(raw_layouts[i]->name, u->name, sizeof(u->name));
    snprintf(u->layout_src, sizeof(u->layout_src), "%s",
             raw_layouts[i]->name ? raw_layouts[i]->name : "default");
    u->ir_def = raw_layouts[i];
    u->kind = RK_LAYOUT;
    snprintf(u->dir, sizeof(u->dir), "%s", "layouts");
    {
      char relbuf[128];
      snprintf(relbuf, sizeof(relbuf), "layouts/%s.jsx", u->name);
      snprintf(u->rel, sizeof(u->rel), "%s", relbuf);
    }
    cord_guess_source_path(u->name, u->dir, u->source_path,
                           sizeof(u->source_path));
    u->use_outlet = 1;
    p->has_router = 1;
    scan_tree_deps_ir(u->ir_def, u);
  }

  if (p->truncated) {
    fprintf(stderr,
            "error: project exceeds React backend limits "
            "(max %d units, %d routes) — split the app or raise limits\n",
            REACT_MAX_UNITS, REACT_MAX_ROUTES);
  }
}

static void import_path_between(const ReactUnit *from, const ReactUnit *to,
                                char *out, size_t n) {
  if (strcmp(from->dir, to->dir) == 0)
    snprintf(out, n, "./%s", to->name);
  else
    snprintf(out, n, "../%s/%s", to->dir, to->name);
}

static void collect_bind_names_ir(IrNode *n, char names[][64], int *count,
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
    collect_bind_names_ir(n->kids[i], names, count, max);
}

static int state_name_exists_ir(IrNode *def, const char *name) {
  if (!def || !name) return 0;
  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (!c) continue;
    if (c->kind == IR_STATE) {
      if (c->name && strcmp(c->name, "__states__") == 0) {
        for (size_t j = 0; j < c->n_kids; j++) {
          if (c->kids[j] && c->kids[j]->name &&
              strcmp(c->kids[j]->name, name) == 0)
            return 1;
        }
      } else if (c->name && strcmp(c->name, name) == 0)
        return 1;
    }
    if (ir_hook_is(c, "action") && c->value && strcmp(c->value, name) == 0)
      return 1;
    if (c->kind == IR_FETCH && c->name && strcmp(c->name, name) == 0) return 1;
  }
  return 0;
}

/* ── element / children emission ────────────────────────── */

static void gen_ir_element(StrBuf *sb, IrNode *node, int depth, GenCtx *ctx) {
  const char *tag = node->name ? node->name : "div";

  if (is_pascal_case(tag)) {
    sb_indent(sb, depth);
    sb_appendf(sb, "<%s", tag);
    for (size_t i = 0; i < node->n_kids; i++) {
      IrNode *child = node->kids[i];
      if (!child) continue;
      if (child->kind == IR_ATTR && child->name) {
        if (is_style_attr_name(child->name) || is_style_bool_name(child->name))
          continue;
        if (strcmp(child->name, "__file__") == 0) continue;
        if (attr_is_true(child->value) && is_style_bool_name(child->name))
          continue;
        if (attr_is_true(child->value) &&
            (strcmp(child->name, "bold") == 0 ||
             strcmp(child->name, "muted") == 0 ||
             strcmp(child->name, "center") == 0 ||
             strcmp(child->name, "between") == 0 ||
             strcmp(child->name, "sticky") == 0 ||
             strcmp(child->name, "primary") == 0 ||
             strcmp(child->name, "outline") == 0 ||
             strcmp(child->name, "ghost") == 0))
          continue;
        if (attr_is_true(child->value) && !is_style_attr_name(child->name) &&
            strcmp(child->name, "required") != 0 &&
            strcmp(child->name, "disabled") != 0 &&
            strcmp(child->name, "readonly") != 0 &&
            strcmp(child->name, "checked") != 0) {
          /* boolean prop without value for true style-like already skipped */
          if (looks_like_bool(child->value) || attr_is_true(child->value)) {
            /* bare bool prop */
            if (child->value && strcmp(child->value, "true") == 0 &&
                !is_style_attr_name(child->name) &&
                !is_style_bool_name(child->name)) {
              /* pass as boolean attribute for component props that are bools */
            }
          }
        }
        if (attr_is_true(child->value) && !child->value[0]) {
          sb_appendf(sb, " %s", child->name);
        } else if (child->value && strcmp(child->value, "true") == 0 &&
                   !strchr(child->name, '=')) {
          /* only emit bare bool if it looks like a prop flag, not style */
          if (!is_style_attr_name(child->name) &&
              !is_style_bool_name(child->name))
            sb_appendf(sb, " %s", child->name);
        } else if (!attr_is_true(child->value) ||
                   (child->value && strcmp(child->value, "true") != 0)) {
          sb_appendf(sb, " %s=", child->name);
          if (is_stringish_prop_name(child->name) && child->value &&
              !interp_has(child->value)) {
            char *esc = js_escape_dq_dup(child->value);
            sb_appendf(sb, "\"%s\"", esc ? esc : "");
            free(esc);
          } else {
            emit_jsx_value(sb, child->value, 0);
          }
        } else if (child->value && strcmp(child->value, "false") == 0) {
          sb_appendf(sb, " %s={false}", child->name);
        }
      } else if (child->kind == IR_EVENT && child->name && child->value) {
        char react_event[64];
        react_event[0] = 'o';
        react_event[1] = 'n';
        react_event[2] = (char)toupper((unsigned char)child->name[0]);
        strncpy(react_event + 3, child->name + 1, sizeof(react_event) - 4);
        react_event[sizeof(react_event) - 1] = '\0';
        if (needs_js_arrow(child->value))
          sb_appendf(sb, " %s={() => %s}", react_event, child->value);
        else
          sb_appendf(sb, " %s={%s}", react_event, child->value);
      }
    }
    int has_kids = 0;
    for (size_t i = 0; i < node->n_kids; i++) {
      IrKind t = node->kids[i]->kind;
      if (t == IR_ELEMENT || t == IR_TEXT || t == IR_FOR || t == IR_IF ||
          t == IR_SLOT || t == IR_INTERP)
        has_kids = 1;
    }
    if (!has_kids) {
      sb_append(sb, " />\n");
      return;
    }
    sb_append(sb, ">\n");
    gen_ir_children(sb, node, depth + 1, ctx);
    sb_indent(sb, depth);
    sb_appendf(sb, "</%s>\n", tag);
    return;
  }

  if (strcmp(tag, "provide") == 0) {
    const char *ctx_name = "Context";
    const char *val = NULL;
    for (size_t i = 0; i < node->n_kids; i++) {
      IrNode *ch = node->kids[i];
      if (ch->kind == IR_ATTR && ch->name) {
        if (strcmp(ch->name, "context") == 0 && ch->value) ctx_name = ch->value;
        if (strcmp(ch->name, "value") == 0) val = ch->value;
      }
    }
    sb_indent(sb, depth);
    sb_appendf(sb, "<%s.Provider", ctx_name);
    if (val) {
      sb_append(sb, " value=");
      emit_jsx_value(sb, val, 1);
    }
    sb_append(sb, ">\n");
    gen_ir_children(sb, node, depth + 1, ctx);
    sb_indent(sb, depth);
    sb_appendf(sb, "</%s.Provider>\n", ctx_name);
    if (ctx) ctx->use_context = 1;
    return;
  }

  if (strcmp(tag, "slot") == 0 || node->kind == IR_SLOT) {
    sb_indent(sb, depth);
    if (ctx && ctx->is_layout) {
      sb_append(sb, "<Outlet />\n");
      ctx->use_router = 1;
    } else {
      sb_append(sb, "{children}\n");
      if (ctx) ctx->use_children = 1;
    }
    return;
  }

  const char *html_tag = html_tag_for(tag);
  const char *base_class = tag_to_div_plus_class(tag);

  if (!html_tag) {
    gen_ir_children(sb, node, depth, ctx);
    return;
  }

  int use_link = 0;
  if (strcmp(tag, "link") == 0) {
    if (ctx && ctx->use_router) {
      use_link = 1;
      /* NavLink sets aria-current="page" for active-route styling */
      html_tag = "NavLink";
    }
  }
  if (ctx) {
    if (strcmp(html_tag, "CordIcon") == 0) ctx->use_cord_icon = 1;
    if (strcmp(html_tag, "CordMotion") == 0) ctx->use_cord_motion = 1;
    if (strcmp(html_tag, "CordChart") == 0) ctx->use_cord_chart = 1;
  }

  sb_indent(sb, depth);
  int self_closing =
      (strcmp(html_tag, "img") == 0 || strcmp(html_tag, "input") == 0);
  sb_appendf(sb, "<%s", html_tag);

  char classes[2048];
  int skip_style_size = (strcmp(html_tag, "CordIcon") == 0 ||
                         strcmp(html_tag, "CordMotion") == 0 ||
                         strcmp(html_tag, "CordChart") == 0);
  if (!skip_style_size)
    collect_classes_ir(classes, sizeof(classes), node, base_class);
  else
    classes[0] = '\0';
  {
    char *cls = classes;
    while (*cls == ' ') cls++;
    if (*cls) sb_appendf(sb, " className=\"%s\"", cls);
  }

  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *child = node->kids[i];
    if (!child || child->kind != IR_ATTR || !child->name) continue;
    if (is_style_attr_name(child->name) &&
        !(skip_style_size &&
          (strcmp(child->name, "size") == 0 || strcmp(child->name, "fade") == 0)))
      continue;
    if (is_style_bool_name(child->name) && attr_is_true(child->value)) continue;
    if (strcmp(child->name, "__file__") == 0) continue;
    if (strcmp(child->name, "style") == 0) continue;

    /* skip bare bool style-ish and input type bools handled later */
    if (attr_is_true(child->value) &&
        (strcmp(child->name, "text") == 0 || strcmp(child->name, "email") == 0 ||
         strcmp(child->name, "password") == 0 ||
         strcmp(child->name, "search") == 0 ||
         strcmp(child->name, "number") == 0 ||
         strcmp(child->name, "required") == 0 ||
         strcmp(child->name, "disabled") == 0 ||
         strcmp(child->name, "readonly") == 0 ||
         strcmp(child->name, "checked") == 0))
      continue;

    /* bare identifier content (bool true non-style) skipped for attrs */
    if (attr_is_true(child->value) && !is_style_attr_name(child->name) &&
        !is_style_bool_name(child->name) &&
        !(skip_style_size &&
          (strcmp(child->name, "fade") == 0 || strcmp(child->name, "data") == 0)) &&
        strcmp(child->name, "lazy") != 0 &&
        strcmp(child->name, "forwardRef") != 0 &&
        strcmp(child->name, "action") != 0 &&
        strcmp(child->name, "bind") != 0 && strcmp(child->name, "ref") != 0 &&
        strcmp(child->name, "to") != 0 && strcmp(child->name, "href") != 0 &&
        strcmp(child->name, "src") != 0 && strcmp(child->name, "alt") != 0 &&
        strcmp(child->name, "type") != 0 &&
        strcmp(child->name, "placeholder") != 0 &&
        strcmp(child->name, "name") != 0 && strcmp(child->name, "value") != 0 &&
        strcmp(child->name, "id") != 0 && strcmp(child->name, "key") != 0 &&
        strcmp(child->name, "rows") != 0 &&
        strcmp(child->name, "layout") != 0) {
      /* treat as content later, not attribute */
      continue;
    }

    if (use_link &&
        (strcmp(child->name, "to") == 0 || strcmp(child->name, "href") == 0)) {
      sb_append(sb, " to=");
      emit_jsx_value(sb, child->value ? child->value : "/", 0);
      /* Exact match for home so "/" does not mark every route active */
      if (child->value && strcmp(child->value, "/") == 0)
        sb_append(sb, " end");
      continue;
    }
    if (strcmp(child->name, "to") == 0) {
      const char *href = child->value ? child->value : "/";
      if (!url_href_is_safe(href)) href = "#";
      char *esc = js_escape_dq_dup(href);
      sb_appendf(sb, " href=\"%s\"", esc ? esc : "#");
      free(esc);
      continue;
    }
    if (strcmp(child->name, "action") == 0 && child->value) {
      sb_appendf(sb, " action={%s}", child->value);
      if (ctx && ctx->action_pending[0] &&
          (strcmp(tag, "form") == 0 || strcmp(html_tag, "form") == 0)) {
        if (ctx->action_handler[0] &&
            strcmp(child->value, ctx->action_handler) == 0)
          ctx->in_form_with_action = 1;
        else {
          size_t al = strlen(child->value);
          if (al > 6 && strcmp(child->value + al - 6, "Action") == 0)
            ctx->in_form_with_action = 1;
        }
      }
    } else if (strcmp(child->name, "src") == 0 ||
               strcmp(child->name, "alt") == 0 ||
               strcmp(child->name, "href") == 0 ||
               strcmp(child->name, "placeholder") == 0 ||
               strcmp(child->name, "type") == 0 ||
               strcmp(child->name, "rows") == 0 ||
               strcmp(child->name, "name") == 0 ||
               strcmp(child->name, "value") == 0 ||
               strcmp(child->name, "id") == 0 ||
               strcmp(child->name, "key") == 0) {
      /* type=display|title|… is typography, not HTML type */
      if (strcmp(child->name, "type") == 0 && child->value &&
          (strcmp(child->value, "display") == 0 ||
           strcmp(child->value, "title") == 0 ||
           strcmp(child->value, "body") == 0 ||
           strcmp(child->value, "caption") == 0 ||
           strcmp(child->value, "code") == 0))
        continue;
      const char *an = child->name;
      if (child->value && interp_has(child->value)) {
        char *body = interp_to_js_template_body(child->value);
        sb_appendf(sb, " %s={`%s`}", an, body ? body : "");
        free(body);
      } else if (child->value && looks_like_js_expr(child->value) &&
                 strchr(child->value, '.')) {
        sb_appendf(sb, " %s={%s}", an, child->value);
      } else if (child->value && looks_like_number(child->value)) {
        sb_appendf(sb, " %s={%s}", an, child->value);
      } else {
        sb_appendf(sb, " %s=\"%s\"", an, child->value ? child->value : "");
      }
    } else if (strcmp(child->name, "bind") == 0 && child->value) {
      const char *bn = child->value;
      char setter[128];
      int has_name = 0;
      for (size_t j = 0; j < node->n_kids; j++) {
        IrNode *a = node->kids[j];
        if (a->kind == IR_ATTR && a->name && strcmp(a->name, "name") == 0)
          has_name = 1;
      }
      if (strchr(bn, '.')) {
        sb_appendf(sb, " value={%s}", bn);
        sb_appendf(sb, " onChange={(e) => { /* bind %s */ }}", bn);
      } else {
        snprintf(setter, sizeof(setter), "set%c%s",
                 (char)toupper((unsigned char)bn[0]), bn + 1);
        sb_appendf(sb, " value={%s}", bn);
        sb_appendf(sb, " onChange={(e) => %s(e.target.value)}", setter);
        if (!has_name) sb_appendf(sb, " name=\"%s\"", bn);
      }
    } else if (strcmp(child->name, "ref") == 0 && child->value) {
      sb_appendf(sb, " ref={%s}", child->value);
    } else {
      sb_appendf(sb, " %s=", child->name);
      emit_jsx_value(sb, child->value, 0);
    }
  }

  if (strcmp(tag, "checkbox") == 0) sb_append(sb, " type=\"checkbox\"");
  if (strcmp(tag, "radio") == 0) sb_append(sb, " type=\"radio\"");
  if (strcmp(tag, "input") == 0) {
    int has_type = 0;
    for (size_t i = 0; i < node->n_kids; i++) {
      IrNode *c = node->kids[i];
      if (c->kind == IR_ATTR && c->name && strcmp(c->name, "type") == 0)
        has_type = 1;
      if (c->kind == IR_ATTR && c->name && attr_is_true(c->value)) {
        if (strcmp(c->name, "text") == 0 || strcmp(c->name, "email") == 0 ||
            strcmp(c->name, "password") == 0 ||
            strcmp(c->name, "search") == 0 || strcmp(c->name, "number") == 0) {
          if (!has_type) {
            sb_appendf(sb, " type=\"%s\"", c->name);
            has_type = 1;
          }
        }
      }
    }
  }

  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *child = node->kids[i];
    if (child->kind == IR_ATTR && child->name && attr_is_true(child->value)) {
      if (strcmp(child->name, "required") == 0 ||
          strcmp(child->name, "disabled") == 0 ||
          strcmp(child->name, "readonly") == 0 ||
          strcmp(child->name, "checked") == 0)
        sb_appendf(sb, " %s", child->name);
    }
  }

  if (ctx && ctx->action_pending[0]) {
    if ((strcmp(tag, "form") == 0 || strcmp(html_tag, "form") == 0) &&
        ctx->in_form_with_action) {
      int has_busy = 0;
      for (size_t i = 0; i < node->n_kids; i++) {
        IrNode *a = node->kids[i];
        if (a->kind == IR_ATTR && a->name &&
            (strcmp(a->name, "aria-busy") == 0 ||
             strcmp(a->name, "ariaBusy") == 0))
          has_busy = 1;
      }
      if (!has_busy)
        sb_appendf(sb, " aria-busy={%s}", ctx->action_pending);
    }
    if ((strcmp(tag, "btn") == 0 || strcmp(html_tag, "button") == 0) &&
        ctx->in_form_with_action) {
      int has_dis = 0;
      for (size_t i = 0; i < node->n_kids; i++) {
        IrNode *a = node->kids[i];
        if (a->kind == IR_ATTR && a->name && strcmp(a->name, "disabled") == 0)
          has_dis = 1;
      }
      if (!has_dis)
        sb_appendf(sb, " disabled={%s}", ctx->action_pending);
    }
  }

  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *child = node->kids[i];
    if (child->kind == IR_EVENT && child->name && child->value) {
      const char *event = child->name;
      const char *handler = child->value;
      char react_event[64];
      react_event[0] = 'o';
      react_event[1] = 'n';
      react_event[2] = (char)toupper((unsigned char)event[0]);
      strncpy(react_event + 3, event + 1, sizeof(react_event) - 4);
      react_event[sizeof(react_event) - 1] = '\0';
      if (strcmp(event, "submit") == 0) {
        if (needs_js_arrow(handler))
          sb_appendf(sb, " %s={(e) => { e.preventDefault(); %s; }}",
                     react_event, handler);
        else
          sb_appendf(sb, " %s={(e) => { e.preventDefault(); %s(e); }}",
                     react_event, handler);
      } else if (needs_js_arrow(handler)) {
        sb_appendf(sb, " %s={() => %s}", react_event, handler);
      } else {
        sb_appendf(sb, " %s={%s}", react_event, handler);
      }
    }
  }

  if (self_closing) {
    sb_append(sb, " />\n");
    return;
  }

  sb_append(sb, ">\n");

  /* Implicit children: bare identifiers (ATTR name=true) as content */
  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *child = node->kids[i];
    if (!child || child->kind != IR_ATTR || !child->name) continue;
    if (!attr_is_true(child->value)) continue;
    const char *v = child->name;
    if (is_style_bool_name(v) || is_style_attr_name(v) ||
        strcmp(v, "required") == 0 || strcmp(v, "disabled") == 0 ||
        strcmp(v, "readonly") == 0 || strcmp(v, "checked") == 0 ||
        strcmp(v, "text") == 0 || strcmp(v, "email") == 0 ||
        strcmp(v, "password") == 0 || strcmp(v, "search") == 0 ||
        strcmp(v, "number") == 0 || strcmp(v, "lazy") == 0 ||
        strcmp(v, "forwardRef") == 0 || strcmp(v, "__file__") == 0 ||
        strcmp(v, "style") == 0 ||
        (skip_style_size &&
         (strcmp(v, "fade") == 0 || strcmp(v, "data") == 0)))
      continue;
    sb_indent(sb, depth + 1);
    sb_appendf(sb, "{%s}\n", v);
  }

  gen_ir_children(sb, node, depth + 1, ctx);
  if (ctx && (strcmp(tag, "form") == 0 || strcmp(html_tag, "form") == 0))
    ctx->in_form_with_action = 0;
  sb_indent(sb, depth);
  sb_appendf(sb, "</%s>\n", html_tag);
}

static void gen_ir_children(StrBuf *sb, IrNode *node, int depth, GenCtx *ctx) {
  for (size_t i = 0; i < node->n_kids; i++) {
    IrNode *child = node->kids[i];
    if (!child) continue;
    if (child->kind == IR_FOR) {
      const char *var = child->name ? child->name : "item";
      const char *list = child->value ? child->value : "items";
      const char *key_expr = "idx";
      for (size_t j = 0; j < child->n_kids; j++) {
        IrNode *ch = child->kids[j];
        if (ch->kind == IR_ATTR && ch->name && strcmp(ch->name, "key") == 0 &&
            ch->value) {
          key_expr = ch->value;
          break;
        }
      }
      sb_indent(sb, depth);
      sb_appendf(sb, "{(%s || []).map((%s, idx) => (\n", list, var);
      sb_indent(sb, depth + 1);
      sb_appendf(sb, "<Fragment key={%s}>\n", key_expr);
      for (size_t j = 0; j < child->n_kids; j++) {
        IrNode *ch = child->kids[j];
        if (ch->kind == IR_ATTR || ch->kind == IR_EVENT) continue;
        gen_ir_node(sb, ch, depth + 2, ctx);
      }
      sb_indent(sb, depth + 1);
      sb_append(sb, "</Fragment>\n");
      sb_indent(sb, depth);
      sb_append(sb, "))}\n");
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
      if (has_else)
        sb_appendf(sb, "{%s ? (\n", cond);
      else
        sb_appendf(sb, "{%s && (\n", cond);
      sb_indent(sb, depth + 1);
      sb_append(sb, "<>\n");
      for (size_t j = 0; j < child->n_kids; j++)
        gen_ir_node(sb, child->kids[j], depth + 2, ctx);
      sb_indent(sb, depth + 1);
      sb_append(sb, "</>\n");
      if (has_else) {
        sb_indent(sb, depth);
        sb_append(sb, ") : (\n");
        sb_indent(sb, depth + 1);
        sb_append(sb, "<>\n");
        IrNode *else_node = node->kids[else_idx];
        for (size_t j = 0; j < else_node->n_kids; j++)
          gen_ir_node(sb, else_node->kids[j], depth + 2, ctx);
        sb_indent(sb, depth + 1);
        sb_append(sb, "</>\n");
        sb_indent(sb, depth);
        sb_append(sb, ")}\n");
        i = else_idx;
      } else {
        sb_indent(sb, depth);
        sb_append(sb, ")}\n");
      }
    } else if (child->kind == IR_SLOT) {
      sb_indent(sb, depth);
      if (ctx && ctx->is_layout) {
        sb_append(sb, "<Outlet />\n");
        if (ctx) ctx->use_router = 1;
      } else {
        sb_append(sb, "{children}\n");
        if (ctx) ctx->use_children = 1;
      }
    } else if (child->kind == IR_INTERP) {
      gen_ir_interpolation(sb, child, depth);
    } else if (child->kind == IR_TEXT) {
      if (child->value && strcmp(child->value, "__else__") == 0) continue;
      sb_indent(sb, depth);
      if (child->value && interp_has(child->value)) {
        char *body = interp_to_js_template_body(child->value);
        sb_appendf(sb, "{`%s`}\n", body ? body : "");
        free(body);
      } else {
        char *plain = interp_plain_text(child->value);
        sb_append(sb, "{'");
        if (plain) {
          for (const char *p = plain; *p; p++) {
            if (*p == '\'' || *p == '\\') sb_append(sb, "\\");
            char ch[2] = {*p, 0};
            sb_append(sb, ch);
          }
        }
        sb_append(sb, "'}\n");
        free(plain);
      }
    } else if (child->kind != IR_ATTR && child->kind != IR_EVENT &&
               child->kind != IR_PROP && child->kind != IR_STATE &&
               child->kind != IR_COMPUTED && child->kind != IR_EFFECT &&
               child->kind != IR_COMPONENT && child->kind != IR_LAYOUT &&
               child->kind != IR_ROUTE && !is_decl_ir(child)) {
      gen_ir_node(sb, child, depth, ctx);
    }
  }
}

static void gen_ir_interpolation(StrBuf *sb, IrNode *node, int depth) {
  if (!node) return;
  if (node->value && node->n_kids == 0) {
    sb_indent(sb, depth);
    sb_appendf(sb, "{%s}\n", node->value);
    return;
  }
  if (node->n_kids > 0) {
    sb_indent(sb, depth);
    sb_append(sb, "{`");
    for (size_t i = 0; i < node->n_kids; i++) {
      IrNode *c = node->kids[i];
      if (c->kind == IR_TEXT && c->value) {
        for (const char *p = c->value; *p; p++) {
          if (*p == '`' || *p == '\\') {
            sb_append(sb, "\\");
            char ch[2] = {*p, 0};
            sb_append(sb, ch);
          } else if (*p == '$' && p[1] == '{') {
            sb_append(sb, "\\$");
          } else {
            char ch[2] = {*p, 0};
            sb_append(sb, ch);
          }
        }
      } else if (c->kind == IR_INTERP && c->value) {
        sb_appendf(sb, "${%s}", c->value);
      }
    }
    sb_append(sb, "`}\n");
  }
}

static void gen_ir_node(StrBuf *sb, IrNode *node, int depth, GenCtx *ctx) {
  if (!node) return;
  switch (node->kind) {
    case IR_ELEMENT:
      gen_ir_element(sb, node, depth, ctx);
      break;
    case IR_SLOT:
      sb_indent(sb, depth);
      if (ctx && ctx->is_layout) {
        sb_append(sb, "<Outlet />\n");
        ctx->use_router = 1;
      } else {
        sb_append(sb, "{children}\n");
        if (ctx) ctx->use_children = 1;
      }
      break;
    case IR_HOOK:
      if (ir_hook_is(node, "suspense") || ir_hook_is(node, "loading")) {
        if (ctx) ctx->use_suspense = 1;
        const char *fb_text = NULL;
        IrNode *fb_node = NULL;
        for (size_t i = 0; i < node->n_kids; i++) {
          IrNode *c = node->kids[i];
          if (c->kind == IR_ATTR && c->name && strcmp(c->name, "fallback") == 0)
            fb_text = c->value;
          if (c->kind == IR_ELEMENT && c->name &&
              strcmp(c->name, "__fallback__") == 0)
            fb_node = c;
        }
        sb_indent(sb, depth);
        sb_append(sb, "<Suspense fallback={");
        if (fb_node) {
          sb_append(sb, "(\n");
          sb_indent(sb, depth + 1);
          sb_append(sb, "<>\n");
          for (size_t i = 0; i < fb_node->n_kids; i++)
            gen_ir_node(sb, fb_node->kids[i], depth + 2, ctx);
          sb_indent(sb, depth + 1);
          sb_append(sb, "</>\n");
          sb_indent(sb, depth);
          sb_append(sb, ")");
        } else if (fb_text) {
          sb_appendf(sb, "<div>%s</div>", fb_text);
        } else {
          sb_append(sb, "<div>Loading...</div>");
        }
        sb_append(sb, "}>\n");
        for (size_t i = 0; i < node->n_kids; i++) {
          IrNode *c = node->kids[i];
          if (c->kind == IR_ATTR) continue;
          if (c->kind == IR_ELEMENT && c->name &&
              strcmp(c->name, "__fallback__") == 0)
            continue;
          gen_ir_node(sb, c, depth + 1, ctx);
        }
        sb_indent(sb, depth);
        sb_append(sb, "</Suspense>\n");
      } else if (ir_hook_is(node, "empty")) {
        const char *cond = node->value ? node->value : "true";
        sb_indent(sb, depth);
        sb_appendf(sb, "{(%s) ? (\n", cond);
        sb_indent(sb, depth + 1);
        sb_append(sb, "<>\n");
        for (size_t i = 0; i < node->n_kids; i++)
          gen_ir_node(sb, node->kids[i], depth + 2, ctx);
        sb_indent(sb, depth + 1);
        sb_append(sb, "</>\n");
        sb_indent(sb, depth);
        sb_append(sb, ") : null}\n");
      } else if (ir_hook_is(node, "portal")) {
        if (ctx) ctx->use_portal = 1;
        sb_indent(sb, depth);
        sb_append(sb, "{createPortal(\n");
        sb_indent(sb, depth + 1);
        sb_append(sb, "<>\n");
        for (size_t i = 0; i < node->n_kids; i++)
          gen_ir_node(sb, node->kids[i], depth + 2, ctx);
        sb_indent(sb, depth + 1);
        sb_append(sb, "</>,\n");
        sb_indent(sb, depth + 1);
        sb_appendf(sb, "%s\n", node->value ? node->value : "document.body");
        sb_indent(sb, depth);
        sb_append(sb, ")}\n");
      } else if (ir_hook_is(node, "errorBoundary")) {
        if (ctx) ctx->use_error_boundary = 1;
        IrNode *fb_node = NULL;
        for (size_t i = 0; i < node->n_kids; i++) {
          IrNode *c = node->kids[i];
          if (c->kind == IR_ELEMENT && c->name &&
              strcmp(c->name, "__fallback__") == 0)
            fb_node = c;
        }
        sb_indent(sb, depth);
        sb_append(sb, "<ErrorBoundary fallback={");
        if (fb_node) {
          sb_append(sb, "(\n");
          sb_indent(sb, depth + 1);
          sb_append(sb, "<>\n");
          for (size_t i = 0; i < fb_node->n_kids; i++)
            gen_ir_node(sb, fb_node->kids[i], depth + 2, ctx);
          sb_indent(sb, depth + 1);
          sb_append(sb, "</>\n");
          sb_indent(sb, depth);
          sb_append(sb, ")");
        } else {
          sb_append(sb, "<div>Something went wrong.</div>");
        }
        sb_append(sb, "}>\n");
        for (size_t i = 0; i < node->n_kids; i++) {
          IrNode *c = node->kids[i];
          if (c->kind == IR_ELEMENT && c->name &&
              strcmp(c->name, "__fallback__") == 0)
            continue;
          gen_ir_node(sb, c, depth + 1, ctx);
        }
        sb_indent(sb, depth);
        sb_append(sb, "</ErrorBoundary>\n");
      }
      break;
    case IR_INTERP:
      gen_ir_interpolation(sb, node, depth);
      break;
    case IR_TEXT:
      if (node->value && strcmp(node->value, "__else__") != 0) {
        sb_indent(sb, depth);
        if (interp_has(node->value)) {
          char *body = interp_to_js_template_body(node->value);
          sb_appendf(sb, "{`%s`}\n", body ? body : "");
          free(body);
        } else {
          char *plain = interp_plain_text(node->value);
          sb_append(sb, "{'");
          if (plain) {
            for (const char *p = plain; *p; p++) {
              if (*p == '\'' || *p == '\\') sb_append(sb, "\\");
              char ch[2] = {*p, 0};
              sb_append(sb, ch);
            }
          }
          sb_append(sb, "'}\n");
          free(plain);
        }
      }
      break;
    case IR_IF:
    case IR_FOR:
      /* handled as siblings in gen_ir_children; still emit if top-level */
      {
        IrNode fake;
        memset(&fake, 0, sizeof(fake));
        IrNode *kids[1] = {node};
        fake.kids = kids;
        fake.n_kids = 1;
        gen_ir_children(sb, &fake, depth, ctx);
      }
      break;
    default:
      gen_ir_children(sb, node, depth, ctx);
      break;
  }
}

/* ── component function from IR ─────────────────────────── */

static void gen_component_fn_ir(StrBuf *sb, IrNode *def, const char *name,
                                GenCtx *ctx) {
  IrNode *props_decl = NULL;
  int has_children_slot = 0;

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (c->kind == IR_PROP && c->name && strcmp(c->name, "__props__") == 0 &&
        !props_decl)
      props_decl = c;
    else if (c->kind == IR_PROP && !c->name && !props_decl)
      props_decl = c;
    if (c->kind == IR_SLOT && !ctx->is_layout) has_children_slot = 1;
    if (c->kind == IR_ELEMENT && c->name && strcmp(c->name, "slot") == 0 &&
        !ctx->is_layout)
      has_children_slot = 1;
    if (c->kind == IR_ATTR && c->name && strcmp(c->name, "forwardRef") == 0 &&
        attr_is_true(c->value))
      ctx->use_forward_ref = 1;
  }
  if (has_children_slot) ctx->use_children = 1;

  sb_appendf(sb, "function %s(", name);
  sb_append(sb, "{ ");
  int prop_n = 0;
  if (props_decl) {
    for (size_t i = 0; i < props_decl->n_kids; i++) {
      IrNode *pr = props_decl->kids[i];
      if (!pr->name) continue;
      if (prop_n++) sb_append(sb, ", ");
      sb_append(sb, pr->name);
      if (pr->value) {
        sb_append(sb, " = ");
        emit_js_literal(sb, pr->value);
      }
    }
  }
  if (ctx->use_children || has_children_slot) {
    if (prop_n++) sb_append(sb, ", ");
    sb_append(sb, "children");
  }
  sb_append(sb, " }");
  if (ctx->use_forward_ref) sb_append(sb, ", ref");
  sb_append(sb, ") {\n");

  /* state bags + leaf states */
  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (c->kind != IR_STATE) continue;
    if (c->name && strcmp(c->name, "__states__") == 0) {
      for (size_t j = 0; j < c->n_kids; j++) {
        IrNode *st = c->kids[j];
        if (!st->name) continue;
        char setter[128];
        snprintf(setter, sizeof(setter), "set%c%s",
                 (char)toupper((unsigned char)st->name[0]), st->name + 1);
        sb_appendf(sb, "  const [%s, %s] = useState(", st->name, setter);
        emit_js_literal(sb, st->value ? st->value : "null");
        sb_append(sb, ");\n");
        ctx->use_state = 1;
      }
    } else if (c->name) {
      char setter[128];
      snprintf(setter, sizeof(setter), "set%c%s",
               (char)toupper((unsigned char)c->name[0]), c->name + 1);
      sb_appendf(sb, "  const [%s, %s] = useState(", c->name, setter);
      emit_js_literal(sb, c->value ? c->value : "null");
      sb_append(sb, ");\n");
      ctx->use_state = 1;
    }
  }

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (!ir_hook_is(c, "reducer") || !c->value) continue;
    const char *init = "undefined";
    for (size_t j = 0; j < c->n_kids; j++) {
      if (c->kids[j]->kind == IR_ATTR && c->kids[j]->name &&
          strcmp(c->kids[j]->name, "init") == 0 && c->kids[j]->value)
        init = c->kids[j]->value;
    }
    sb_appendf(sb, "  const [%s, dispatch] = useReducer(%s, %s);\n", c->value,
               c->value2 ? c->value2 : "reducer", init);
    ctx->use_reducer = 1;
  }

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (ir_hook_is(c, "ctx") && c->value && c->value2) {
      sb_appendf(sb, "  const %s = useContext(%s);\n", c->value, c->value2);
      ctx->use_context = 1;
    }
  }

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (ir_hook_is(c, "ref") && c->value) {
      sb_appendf(sb, "  const %s = useRef(", c->value);
      if (c->value2)
        emit_js_literal(sb, c->value2);
      else
        sb_append(sb, "null");
      sb_append(sb, ");\n");
      ctx->use_ref = 1;
    }
  }

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (ir_hook_is(c, "params")) {
      sb_append(sb, "  const { ");
      sb_append(sb, c->value ? c->value : "id");
      sb_append(sb, " } = useParams();\n");
      ctx->use_params = 1;
    }
  }

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (ir_hook_is(c, "navigate")) {
      sb_appendf(sb, "  const %s = useNavigate();\n",
                 c->value ? c->value : "navigate");
      ctx->use_navigate = 1;
    }
  }

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (ir_hook_is(c, "id") && c->value) {
      sb_appendf(sb, "  const %s = useId();\n", c->value);
      ctx->use_id = 1;
    }
  }

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (ir_hook_is(c, "transition")) {
      sb_appendf(sb, "  const [%s, %s] = useTransition();\n",
                 c->value ? c->value : "isPending",
                 c->value2 ? c->value2 : "startTransition");
      ctx->use_transition = 1;
    }
  }

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (ir_hook_is(c, "deferred") && c->value) {
      sb_appendf(sb, "  const %s = useDeferredValue(%s);\n", c->value,
                 c->value2 ? c->value2 : c->value);
      ctx->use_deferred = 1;
    }
  }

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (c->kind == IR_COMPUTED && c->name) {
      sb_appendf(sb, "  const %s = useMemo(() => (%s), []);\n", c->name,
                 c->value ? c->value : "null");
      ctx->use_memo = 1;
    }
  }

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (!ir_hook_is(c, "callback") || !c->value) continue;
    const char *deps = "";
    for (size_t j = 0; j < c->n_kids; j++) {
      if (c->kids[j]->kind == IR_ATTR && c->kids[j]->name &&
          strcmp(c->kids[j]->name, "deps") == 0 && c->kids[j]->value)
        deps = c->kids[j]->value;
    }
    const char *fn = c->value2 ? c->value2 : "() => {}";
    sb_appendf(sb, "  const %s = useCallback(%s, [%s]);\n", c->value, fn, deps);
    ctx->use_callback = 1;
  }

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (c->kind != IR_EFFECT) continue;
    const char *deps = c->value ? c->value : "";
    const char *body = c->value2 ? c->value2 : "";
    const char *cleanup = NULL;
    for (size_t j = 0; j < c->n_kids; j++) {
      if (c->kids[j]->kind == IR_ATTR && c->kids[j]->name &&
          strcmp(c->kids[j]->name, "cleanup") == 0)
        cleanup = c->kids[j]->value;
    }
    const char *hook = "useEffect";
    if (c->name && strcmp(c->name, "layoutEffect") == 0)
      hook = "useLayoutEffect";
    else if (c->name && strcmp(c->name, "insertionEffect") == 0)
      hook = "useInsertionEffect";
    sb_appendf(sb, "  %s(() => {\n", hook);
    if (body && body[0]) sb_appendf(sb, "    %s;\n", body);
    if (cleanup && cleanup[0]) {
      sb_append(sb, "    return () => {\n");
      sb_appendf(sb, "      %s;\n", cleanup);
      sb_append(sb, "    };\n");
    }
    sb_appendf(sb, "  }, [%s]);\n", deps);
    if (c->name && strcmp(c->name, "layoutEffect") == 0)
      ctx->use_layout_effect = 1;
    else if (c->name && strcmp(c->name, "insertionEffect") == 0)
      ctx->use_insertion_effect = 1;
    else
      ctx->use_effect = 1;
  }

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (!ir_hook_is(c, "effectEvent") || !c->value) continue;
    const char *fn = c->value2 ? c->value2 : "() => {}";
    int is_fn = (fn[0] == '(' || strncmp(fn, "function", 8) == 0 ||
                 strstr(fn, "=>") != NULL);
    if (is_fn) {
      sb_appendf(sb, "  const %s = useEffectEvent(%s);\n", c->value, fn);
    } else {
      int simple_id = 1;
      for (const char *p = fn; *p; p++) {
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
              (*p >= '0' && *p <= '9') || *p == '_' || *p == '$')) {
          simple_id = 0;
          break;
        }
      }
      if (simple_id)
        sb_appendf(sb, "  const %s = useEffectEvent(%s);\n", c->value, fn);
      else
        sb_appendf(sb, "  const %s = useEffectEvent((..._args) => { %s; });\n",
                   c->value, fn);
    }
    ctx->use_effect_event = 1;
  }

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (!ir_hook_is(c, "externalStore") || !c->value) continue;
    const char *sub = c->value2 ? c->value2 : "() => () => {}";
    const char *get_snap = "() => null";
    const char *get_server = NULL;
    for (size_t j = 0; j < c->n_kids; j++) {
      if (c->kids[j]->kind != IR_ATTR || !c->kids[j]->name) continue;
      if (strcmp(c->kids[j]->name, "getSnapshot") == 0 && c->kids[j]->value)
        get_snap = c->kids[j]->value;
      if (strcmp(c->kids[j]->name, "getServerSnapshot") == 0 &&
          c->kids[j]->value)
        get_server = c->kids[j]->value;
    }
    if (get_server && get_server[0] && strcmp(get_server, "null") != 0)
      sb_appendf(sb, "  const %s = useSyncExternalStore(%s, %s, %s);\n",
                 c->value, sub, get_snap, get_server);
    else
      sb_appendf(sb, "  const %s = useSyncExternalStore(%s, %s);\n", c->value,
                 sub, get_snap);
    ctx->use_sync_external_store = 1;
  }

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (!ir_hook_is(c, "imperativeHandle")) continue;
    const char *refn = c->value ? c->value : "ref";
    const char *body = c->value2 ? c->value2 : "";
    const char *deps = "";
    for (size_t j = 0; j < c->n_kids; j++) {
      if (c->kids[j]->kind == IR_ATTR && c->kids[j]->name &&
          strcmp(c->kids[j]->name, "deps") == 0 && c->kids[j]->value)
        deps = c->kids[j]->value;
      if (c->kids[j]->kind == IR_ATTR && c->kids[j]->name &&
          strcmp(c->kids[j]->name, "ref") == 0 && c->kids[j]->value)
        refn = c->kids[j]->value;
    }
    sb_appendf(sb, "  useImperativeHandle(%s, () => ({\n", refn);
    if (body && body[0]) sb_appendf(sb, "    %s\n", body);
    sb_appendf(sb, "  }), [%s]);\n", deps);
    ctx->use_imperative_handle = 1;
  }

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (!ir_hook_is(c, "action") || !c->value) continue;
    const char *init = "null";
    const char *pending = NULL;
    for (size_t j = 0; j < c->n_kids; j++) {
      if (c->kids[j]->kind != IR_ATTR || !c->kids[j]->name) continue;
      if (strcmp(c->kids[j]->name, "init") == 0 && c->kids[j]->value)
        init = c->kids[j]->value;
      if (strcmp(c->kids[j]->name, "pending") == 0 && c->kids[j]->value)
        pending = c->kids[j]->value;
    }
    char action_name[128], pending_name[128];
    snprintf(action_name, sizeof(action_name), "%sAction", c->value);
    if (pending)
      snprintf(pending_name, sizeof(pending_name), "%s", pending);
    else
      snprintf(pending_name, sizeof(pending_name), "%sPending", c->value);
    sb_appendf(sb, "  const [%s, %s, %s] = useActionState(%s, %s);\n", c->value,
               action_name, pending_name,
               c->value2 ? c->value2 : "async () => null", init);
    ctx->use_action = 1;
    if (!ctx->action_pending[0]) {
      snprintf(ctx->action_pending, sizeof(ctx->action_pending), "%s",
               pending_name);
      snprintf(ctx->action_handler, sizeof(ctx->action_handler), "%s",
               action_name);
      if (c->value2)
        snprintf(ctx->action_fn, sizeof(ctx->action_fn), "%s", c->value2);
    }
  }

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (c->kind != IR_FETCH || !c->name) continue;
    const char *nm = c->name;
    const char *url = c->value ? c->value : "/";
    char set_n[128], set_err[128], set_load[128];
    snprintf(set_n, sizeof(set_n), "set%c%s",
             (char)toupper((unsigned char)nm[0]), nm + 1);
    snprintf(set_err, sizeof(set_err), "set%c%sError",
             (char)toupper((unsigned char)nm[0]), nm + 1);
    snprintf(set_load, sizeof(set_load), "set%c%sLoading",
             (char)toupper((unsigned char)nm[0]), nm + 1);
    sb_appendf(sb, "  const [%s, %s] = useState(null);\n", nm, set_n);
    sb_appendf(sb, "  const [%sError, %s] = useState(null);\n", nm, set_err);
    sb_appendf(sb, "  const [%sLoading, %s] = useState(true);\n", nm, set_load);
    sb_append(sb, "  useEffect(() => {\n");
    sb_append(sb, "    let cancelled = false;\n");
    sb_appendf(sb, "    %s(true);\n", set_load);
    sb_appendf(sb, "    %s(null);\n", set_err);
    {
      const char *safe_url = url_href_is_safe(url) ? url : "/";
      char *esc = js_escape_dq_dup(safe_url);
      sb_appendf(sb, "    fetch(\"%s\")\n", esc ? esc : "/");
      free(esc);
    }
    sb_append(sb, "      .then((r) => {\n");
    sb_append(sb, "        if (!r.ok) throw new Error(String(r.status));\n");
    sb_append(sb, "        return r.json();\n");
    sb_append(sb, "      })\n");
    sb_appendf(sb, "      .then((data) => { if (!cancelled) %s(data); })\n",
               set_n);
    sb_appendf(sb, "      .catch((err) => { if (!cancelled) %s(err); })\n",
               set_err);
    sb_appendf(sb, "      .finally(() => { if (!cancelled) %s(false); });\n",
               set_load);
    sb_append(sb, "    return () => { cancelled = true; };\n");
    sb_append(sb, "  }, []);\n");
    ctx->use_state = 1;
    ctx->use_effect = 1;
  }

  {
    char binds[32][64];
    int n_binds = 0;
    collect_bind_names_ir(def, binds, &n_binds, 32);
    for (int bi = 0; bi < n_binds; bi++) {
      if (state_name_exists_ir(def, binds[bi])) continue;
      char setter[128];
      snprintf(setter, sizeof(setter), "set%c%s",
               (char)toupper((unsigned char)binds[bi][0]), binds[bi] + 1);
      sb_appendf(sb, "  const [%s, %s] = useState(\"\");\n", binds[bi], setter);
      ctx->use_state = 1;
    }
  }

  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (!ir_hook_is(c, "head") || !c->value) continue;
    sb_append(sb, "  useEffect(() => {\n");
    sb_appendf(sb, "    document.title = \"%s\";\n", c->value);
    sb_append(sb, "  }, []);\n");
    ctx->use_effect = 1;
  }

  sb_append(sb, "  return (\n");
  sb_append(sb, "    <>\n");
  int body_count = 0;
  for (size_t i = 0; i < def->n_kids; i++) {
    IrNode *c = def->kids[i];
    if (is_decl_ir(c)) continue;
    gen_ir_node(sb, c, 3, ctx);
    body_count++;
  }
  if (body_count == 0) sb_append(sb, "      null\n");
  sb_append(sb, "    </>\n");
  sb_append(sb, "  );\n");
  sb_append(sb, "}\n");
}

/* ── modules ────────────────────────────────────────────── */

static char *gen_unit_module_ir(ReactProject *proj, ReactUnit *u) {
  GenCtx ctx = {0};
  ctx.use_router = proj->has_router || u->use_link || u->use_outlet;
  ctx.in_component = 1;
  ctx.is_layout = (u->kind == RK_LAYOUT);
  ctx.use_children = u->use_children;

  StrBuf body;
  sb_init(&body);
  gen_component_fn_ir(&body, u->ir_def, u->name, &ctx);

  if (ctx.use_state) u->use_state = 1;
  if (ctx.use_memo) u->use_memo = 1;
  if (ctx.use_effect) u->use_effect = 1;
  if (ctx.use_ref) u->use_ref = 1;
  if (ctx.use_context) u->use_context = 1;
  if (ctx.use_reducer) u->use_reducer = 1;
  if (ctx.use_params) u->use_params = 1;
  if (ctx.use_navigate) u->use_navigate = 1;
  if (ctx.use_callback) u->use_callback = 1;
  if (ctx.use_id) u->use_id = 1;
  if (ctx.use_transition) u->use_transition = 1;
  if (ctx.use_deferred) u->use_deferred = 1;
  if (ctx.use_action) u->use_action = 1;
  if (ctx.use_layout_effect) u->use_layout_effect = 1;
  if (ctx.use_insertion_effect) u->use_insertion_effect = 1;
  if (ctx.use_effect_event) u->use_effect_event = 1;
  if (ctx.use_sync_external_store) u->use_sync_external_store = 1;
  if (ctx.use_imperative_handle) u->use_imperative_handle = 1;
  if (ctx.use_forward_ref) u->use_forward_ref = 1;
  if (ctx.use_suspense) u->use_suspense = 1;
  if (ctx.use_portal) u->use_portal = 1;
  if (ctx.use_error_boundary) u->use_error_boundary = 1;
  if (ctx.use_children) u->use_children = 1;
  if (u->use_link || u->use_outlet || ctx.use_params || ctx.use_navigate)
    ctx.use_router = 1;

  StrBuf out;
  sb_init(&out);

  if (u->source_path[0])
    sb_appendf(&out, "/* cordlang: source=%s */\n", u->source_path);

  {
    const char *hooks[32];
    int nh = 0;
    if (u->use_state || ctx.use_state) hooks[nh++] = "useState";
    if (u->use_memo || ctx.use_memo) hooks[nh++] = "useMemo";
    if (u->use_effect || ctx.use_effect) hooks[nh++] = "useEffect";
    if (u->use_layout_effect || ctx.use_layout_effect)
      hooks[nh++] = "useLayoutEffect";
    if (u->use_insertion_effect || ctx.use_insertion_effect)
      hooks[nh++] = "useInsertionEffect";
    if (u->use_effect_event || ctx.use_effect_event)
      hooks[nh++] = "useEffectEvent";
    if (u->use_sync_external_store || ctx.use_sync_external_store)
      hooks[nh++] = "useSyncExternalStore";
    if (u->use_ref || ctx.use_ref) hooks[nh++] = "useRef";
    if (u->use_imperative_handle || ctx.use_imperative_handle)
      hooks[nh++] = "useImperativeHandle";
    if (u->use_forward_ref || ctx.use_forward_ref) hooks[nh++] = "forwardRef";
    if (u->use_context || ctx.use_context) hooks[nh++] = "useContext";
    if (u->use_reducer || ctx.use_reducer) hooks[nh++] = "useReducer";
    if (u->use_callback || ctx.use_callback) hooks[nh++] = "useCallback";
    if (u->use_id || ctx.use_id) hooks[nh++] = "useId";
    if (u->use_transition || ctx.use_transition) hooks[nh++] = "useTransition";
    if (u->use_deferred || ctx.use_deferred) hooks[nh++] = "useDeferredValue";
    if (u->use_action || ctx.use_action) hooks[nh++] = "useActionState";
    if (u->use_lazy || u->n_lazy > 0) hooks[nh++] = "lazy";
    if (u->use_suspense || ctx.use_suspense) hooks[nh++] = "Suspense";
    if (u->use_portal || ctx.use_portal) hooks[nh++] = "createPortal";
    if (u->use_fragment) hooks[nh++] = "Fragment";
    if (nh > 0) {
      sb_append(&out, "import { ");
      for (int i = 0; i < nh; i++) {
        if (i) sb_append(&out, ", ");
        sb_append(&out, hooks[i]);
      }
      sb_append(&out, " } from 'react';\n");
    }
  }

  if (u->use_error_boundary || ctx.use_error_boundary)
    sb_append(&out, "import ErrorBoundary from '../ErrorBoundary';\n");
  if (ctx.use_cord_icon)
    sb_append(&out, "import CordIcon from '../CordIcon';\n");
  if (ctx.use_cord_motion)
    sb_append(&out, "import CordMotion from '../CordMotion';\n");
  if (ctx.use_cord_chart)
    sb_append(&out, "import CordChart from '../CordChart';\n");

  for (int i = 0; i < u->n_lazy; i++) {
    char ipath[256];
    const char *path = u->lazy_paths[i];
    if (strncmp(path, "pages/", 6) == 0)
      snprintf(ipath, sizeof(ipath), "../pages/%s", path + 6);
    else if (strncmp(path, "components/", 11) == 0)
      snprintf(ipath, sizeof(ipath), "../components/%s", path + 11);
    else if (strncmp(path, "layouts/", 8) == 0)
      snprintf(ipath, sizeof(ipath), "../layouts/%s", path + 8);
    else
      snprintf(ipath, sizeof(ipath), "./%s", path);
    size_t pl = strlen(ipath);
    if (pl > 5 && strcmp(ipath + pl - 5, ".cord") == 0) ipath[pl - 5] = '\0';
    sb_appendf(&out, "const %s = lazy(() => import('%s'));\n", u->lazy_names[i],
               ipath);
  }

  if (u->use_link || u->use_outlet || ctx.use_params || ctx.use_navigate) {
    sb_append(&out, "import { ");
    int first = 1;
    if (u->use_link) {
      sb_append(&out, "NavLink");
      first = 0;
    }
    if (u->use_outlet) {
      if (!first) sb_append(&out, ", ");
      sb_append(&out, "Outlet");
      first = 0;
    }
    if (ctx.use_params || u->use_params) {
      if (!first) sb_append(&out, ", ");
      sb_append(&out, "useParams");
      first = 0;
    }
    if (ctx.use_navigate || u->use_navigate) {
      if (!first) sb_append(&out, ", ");
      sb_append(&out, "useNavigate");
    }
    sb_append(&out, " } from 'react-router-dom';\n");
  }

  if (proj->n_contexts > 0 && (u->use_context || ctx.use_context)) {
    sb_append(&out, "import { ");
    for (int i = 0; i < proj->n_contexts; i++) {
      if (i) sb_append(&out, ", ");
      sb_append(&out,
                proj->contexts[i]->value ? proj->contexts[i]->value : "Ctx");
    }
    sb_append(&out, " } from '../contexts';\n");
  }

  for (int i = 0; i < u->n_used; i++) {
    ReactUnit *dep = project_find_unit(proj, u->used[i]);
    if (dep) {
      char ipath[256];
      import_path_between(u, dep, ipath, sizeof(ipath));
      sb_appendf(&out, "import %s from '%s';\n", dep->name, ipath);
      continue;
    }
    /* foreign multi-target: prefer react binding */
    for (int f = 0; f < proj->n_foreigns; f++) {
      IrNode *fn = proj->foreigns[f];
      if (!fn || !fn->name || strcmp(fn->name, u->used[i]) != 0) continue;
      const char *mod = fn->value; /* default module */
      for (size_t k = 0; k < fn->n_kids; k++) {
        IrNode *a = fn->kids[k];
        if (a && a->kind == IR_ATTR && a->name && a->value &&
            strcmp(a->name, "react") == 0)
          mod = a->value;
      }
      if (mod && mod[0])
        sb_appendf(&out, "import %s from '%s';\n", fn->name, mod);
      break;
    }
  }

  if (out.len > 0) sb_append(&out, "\n");

  for (int si = 0; si < u->n_action_stubs; si++) {
    sb_appendf(&out,
               "/* cordlang: default action stub - replace with your server "
               "action */\n"
               "async function %s(prev, formData) {\n"
               "  return null;\n"
               "}\n\n",
               u->action_stubs[si]);
  }

  if (u->use_forward_ref || ctx.use_forward_ref) {
    sb_append(&out, "export default forwardRef(");
    if (body.buf) {
      size_t bl = strlen(body.buf);
      while (bl > 0 &&
             (body.buf[bl - 1] == '\n' || body.buf[bl - 1] == '\r')) {
        body.buf[--bl] = '\0';
      }
      sb_append(&out, body.buf);
    } else {
      sb_append(&out, "function Empty(props, ref){ return null; }");
    }
    sb_append(&out, ");\n");
  } else {
    sb_append(&out, "export default ");
    sb_append(&out, body.buf ? body.buf : "function Empty(){ return null; }\n");
  }

  free(body.buf);
  return out.buf;
}

static char *gen_contexts_module_ir(ReactProject *proj) {
  if (proj->n_contexts == 0) return NULL;
  StrBuf out;
  sb_init(&out);
  sb_append(&out, "import { createContext } from 'react';\n\n");
  for (int i = 0; i < proj->n_contexts; i++) {
    IrNode *c = proj->contexts[i];
    const char *name = c->value ? c->value : "AppContext";
    sb_appendf(&out, "export const %s = createContext(", name);
    if (c->value2)
      emit_js_literal(&out, c->value2);
    else
      sb_append(&out, "null");
    sb_append(&out, ");\n");
  }
  return out.buf;
}

static const char *route_layout_attr_ir(IrNode *route) {
  if (!route) return NULL;
  for (size_t i = 0; i < route->n_kids; i++) {
    IrNode *c = route->kids[i];
    if (c && c->kind == IR_ATTR && c->name && strcmp(c->name, "layout") == 0 &&
        c->value && c->value[0])
      return c->value;
  }
  return NULL;
}

static ReactUnit *find_layout_for_route_ir(ReactProject *proj, IrNode *route) {
  const char *attr = route_layout_attr_ir(route);
  char export_name[96];

  if (attr) {
    layout_export_name(attr, export_name, sizeof(export_name));
    for (int i = 0; i < proj->n_units; i++) {
      if (proj->units[i].kind != RK_LAYOUT) continue;
      if (strcmp(proj->units[i].name, export_name) == 0)
        return &proj->units[i];
      if (proj->units[i].layout_src[0] &&
          strcmp(proj->units[i].layout_src, attr) == 0)
        return &proj->units[i];
    }
    return NULL;
  }

  ReactUnit *first = NULL;
  for (int i = 0; i < proj->n_units; i++) {
    if (proj->units[i].kind != RK_LAYOUT) continue;
    if (!first) first = &proj->units[i];
    if (strcmp(proj->units[i].name, "DefaultLayout") == 0)
      return &proj->units[i];
    if (strcmp(proj->units[i].layout_src, "default") == 0)
      return &proj->units[i];
  }
  return first;
}

static char *gen_app_module_ir(ReactProject *proj) {
  StrBuf out;
  sb_init(&out);
  sb_append(&out, "/* cordlang: source=src/app.cord */\n");

  int imported[256];
  memset(imported, 0, sizeof(imported));

  int any_lazy_route = 0;
  for (int r = 0; r < proj->n_routes; r++) {
    if (route_is_lazy_ir(proj, proj->routes[r])) {
      any_lazy_route = 1;
      break;
    }
  }

  if (proj->n_routes > 0 || proj->has_router) {
    sb_append(&out,
              "import { BrowserRouter, Routes, Route } from 'react-router-dom';\n");
    if (any_lazy_route)
      sb_append(&out, "import { lazy, Suspense } from 'react';\n");

    for (int i = 0; i < proj->n_units; i++) {
      if (proj->units[i].kind == RK_LAYOUT) {
        sb_appendf(&out, "import %s from './layouts/%s';\n",
                   proj->units[i].name, proj->units[i].name);
        imported[i] = 1;
      }
    }

    for (int r = 0; r < proj->n_routes; r++) {
      const char *comp = proj->routes[r]->value;
      if (!comp) continue;
      if (route_is_lazy_ir(proj, proj->routes[r])) continue;
      for (int i = 0; i < proj->n_units; i++) {
        if (imported[i]) continue;
        if (strcmp(proj->units[i].name, comp) == 0) {
          sb_appendf(&out, "import %s from './%s/%s';\n", proj->units[i].name,
                     proj->units[i].dir, proj->units[i].name);
          imported[i] = 1;
          break;
        }
      }
    }

    for (int r = 0; r < proj->n_routes; r++) {
      const char *comp = proj->routes[r]->value;
      if (!comp || !route_is_lazy_ir(proj, proj->routes[r])) continue;
      int already = 0;
      for (int i = 0; i < proj->n_units; i++) {
        if (imported[i] && strcmp(proj->units[i].name, comp) == 0) already = 1;
      }
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
      sb_appendf(&out, "const %s = lazy(() => import('./%s/%s'));\n", comp, dir,
                 comp);
    }
  } else if (proj->n_units > 0) {
    ReactUnit *root = NULL;
    for (int i = 0; i < proj->n_units; i++) {
      if (proj->units[i].kind == RK_PAGE) {
        root = &proj->units[i];
        break;
      }
    }
    if (!root) root = &proj->units[0];
    sb_appendf(&out, "import %s from './%s/%s';\n", root->name, root->dir,
               root->name);
  }

  sb_append(&out, "\nexport default function App() {\n");

  if (proj->n_routes > 0) {
    sb_append(&out, "  return (\n");
    sb_append(&out, "    <BrowserRouter>\n");
    if (any_lazy_route)
      sb_append(&out,
                "      <Suspense fallback={<div>Loading...</div>}>\n");
    sb_append(&out, "      <Routes>\n");

    int route_done[REACT_MAX_ROUTES];
    memset(route_done, 0, sizeof(route_done));

    for (int r = 0; r < proj->n_routes; r++) {
      if (route_done[r]) continue;
      ReactUnit *layout = find_layout_for_route_ir(proj, proj->routes[r]);
      if (layout) {
        sb_appendf(&out, "        <Route element={<%s />}>\n", layout->name);
        for (int r2 = 0; r2 < proj->n_routes; r2++) {
          if (route_done[r2]) continue;
          if (find_layout_for_route_ir(proj, proj->routes[r2]) != layout)
            continue;
          const char *path =
              proj->routes[r2]->name ? proj->routes[r2]->name : "/";
          const char *comp =
              proj->routes[r2]->value ? proj->routes[r2]->value : "Home";
          sb_appendf(&out, "          <Route path=\"%s\" element={<%s />} />\n",
                     path, comp);
          route_done[r2] = 1;
        }
        sb_append(&out, "        </Route>\n");
      } else {
        const char *path =
            proj->routes[r]->name ? proj->routes[r]->name : "/";
        const char *comp =
            proj->routes[r]->value ? proj->routes[r]->value : "Home";
        sb_appendf(&out, "        <Route path=\"%s\" element={<%s />} />\n",
                   path, comp);
        route_done[r] = 1;
      }
    }

    sb_append(&out, "      </Routes>\n");
    if (any_lazy_route) sb_append(&out, "      </Suspense>\n");
    sb_append(&out, "    </BrowserRouter>\n");
    sb_append(&out, "  );\n");
  } else if (proj->n_page_nodes > 0 && proj->n_units == 0) {
    GenCtx ctx = {0};
    sb_append(&out, "  return (\n");
    if (proj->n_page_nodes > 1) sb_append(&out, "    <>\n");
    for (int i = 0; i < proj->n_page_nodes; i++) {
      gen_ir_node(&out, proj->page_nodes[i],
                  proj->n_page_nodes > 1 ? 3 : 2, &ctx);
    }
    if (proj->n_page_nodes > 1) sb_append(&out, "    </>\n");
    sb_append(&out, "  );\n");
  } else if (proj->n_units > 0) {
    ReactUnit *root = NULL;
    for (int i = 0; i < proj->n_units; i++) {
      if (proj->units[i].kind == RK_PAGE) {
        root = &proj->units[i];
        break;
      }
    }
    if (!root) root = &proj->units[0];
    sb_appendf(&out, "  return <%s />;\n", root->name);
  } else {
    sb_append(&out, "  return null;\n");
  }

  sb_append(&out, "}\n");
  return out.buf;
}

static void attach_root_lazies_ir(ReactProject *proj, IrNode *root) {
  if (!root) return;
  for (size_t i = 0; i < root->n_kids; i++) {
    IrNode *c = root->kids[i];
    if (!ir_hook_is(c, "lazy") || !c->value || !c->value2) continue;
    if (is_route_target_ir(c->value, proj->routes, proj->n_routes)) continue;
    int attached = 0;
    for (int u = 0; u < proj->n_units; u++) {
      int uses = 0;
      for (int k = 0; k < proj->units[u].n_used; k++) {
        if (strcmp(proj->units[u].used[k], c->value) == 0) uses = 1;
      }
      if (uses && proj->units[u].n_lazy < 16) {
        snprintf(proj->units[u].lazy_names[proj->units[u].n_lazy],
                 sizeof(proj->units[u].lazy_names[0]), "%s", c->value);
        snprintf(proj->units[u].lazy_paths[proj->units[u].n_lazy],
                 sizeof(proj->units[u].lazy_paths[0]), "%s", c->value2);
        proj->units[u].n_lazy++;
        proj->units[u].use_lazy = 1;
        attached = 1;
      }
    }
    if (!attached && proj->n_units > 0 && proj->units[0].n_lazy < 16) {
      int u = 0;
      for (int j = 0; j < proj->n_units; j++) {
        if (proj->units[j].kind != RK_LAYOUT) {
          u = j;
          break;
        }
      }
      snprintf(proj->units[u].lazy_names[proj->units[u].n_lazy],
               sizeof(proj->units[u].lazy_names[0]), "%s", c->value);
      snprintf(proj->units[u].lazy_paths[proj->units[u].n_lazy],
               sizeof(proj->units[u].lazy_paths[0]), "%s", c->value2);
      proj->units[u].n_lazy++;
      proj->units[u].use_lazy = 1;
    }
  }
}

int react_emit_modules_from_ir(IrProgram *ir, ReactWriteFn write_fn,
                               void *userdata) {
  if (!ir || !ir->root) return -1;
  ReactProject *proj = calloc(1, sizeof(ReactProject));
  if (!proj) return -1;
  project_partition_from_ir(proj, ir);
  if (proj->truncated) {
    free(proj);
    return -1;
  }
  attach_root_lazies_ir(proj, ir->root);

  for (int i = 0; i < proj->n_units; i++) {
    IrNode *def = proj->units[i].ir_def;
    if (!def) continue;
    for (size_t j = 0; j < def->n_kids; j++) {
      IrNode *c = def->kids[j];
      if (ir_hook_is(c, "context") && c->value && proj->n_contexts < 32) {
        int dup = 0;
        for (int k = 0; k < proj->n_contexts; k++) {
          if (proj->contexts[k]->value &&
              strcmp(proj->contexts[k]->value, c->value) == 0)
            dup = 1;
        }
        if (!dup) proj->contexts[proj->n_contexts++] = c;
      }
    }
  }

  if (proj->n_contexts > 0) {
    char *ctxm = gen_contexts_module_ir(proj);
    if (ctxm) {
      int rc = write_fn("contexts.jsx", ctxm, userdata);
      free(ctxm);
      if (rc != 0) {
        free(proj);
        return rc;
      }
    }
  }

  for (int i = 0; i < proj->n_units; i++) {
    char *mod = gen_unit_module_ir(proj, &proj->units[i]);
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

  char *app = gen_app_module_ir(proj);
  if (!app) {
    free(proj);
    return -1;
  }
  int rc = write_fn("App.jsx", app, userdata);
  free(app);
  free(proj);
  return rc;
}

static char *react_generate_impl_ir(IrProgram *ir) {
  if (!ir || !ir->root)
    return strdup("export default function App(){return null}\n");

  StrBuf sb;
  sb_init(&sb);

  ReactProject *proj = calloc(1, sizeof(ReactProject));
  if (!proj) return strdup("export default function App(){return null}\n");
  project_partition_from_ir(proj, ir);
  if (proj->truncated) {
    free(proj);
    return NULL;
  }
  attach_root_lazies_ir(proj, ir->root);

  for (int i = 0; i < proj->n_units; i++) {
    IrNode *def = proj->units[i].ir_def;
    if (!def) continue;
    for (size_t j = 0; j < def->n_kids; j++) {
      IrNode *c = def->kids[j];
      if (ir_hook_is(c, "context") && c->value && proj->n_contexts < 32) {
        int dup = 0;
        for (int k = 0; k < proj->n_contexts; k++) {
          if (proj->contexts[k]->value &&
              strcmp(proj->contexts[k]->value, c->value) == 0)
            dup = 1;
        }
        if (!dup) proj->contexts[proj->n_contexts++] = c;
      }
    }
  }

  if (proj->n_contexts > 0) {
    char *ctxm = gen_contexts_module_ir(proj);
    sb_append(&sb, "// ===== src/contexts.jsx =====\n");
    sb_append(&sb, ctxm ? ctxm : "");
    sb_append(&sb, "\n");
    free(ctxm);
  }

  for (int i = 0; i < proj->n_units; i++) {
    char *mod = gen_unit_module_ir(proj, &proj->units[i]);
    sb_appendf(&sb, "// ===== src/%s =====\n", proj->units[i].rel);
    sb_append(&sb, mod ? mod : "");
    sb_append(&sb, "\n");
    free(mod);
  }
  char *app = gen_app_module_ir(proj);
  sb_append(&sb, "// ===== src/App.jsx =====\n");
  sb_append(&sb, app ? app : "");
  free(app);
  free(proj);
  return sb.buf;
}

char *react_generate_from_ir(IrProgram *ir) {
  /* Pure IrNode walk — never uses ir_project_origin for body codegen. */
  if (!ir || !ir->root)
    return strdup("export default function App(){return null}\n");
  return react_generate_impl_ir(ir);
}
