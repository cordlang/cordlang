#ifndef CORDLANG_IR_WALK_H
#define CORDLANG_IR_WALK_H

#include "domain/ir.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* Shared IR helpers for IR-2 pure walkers (no AST origin). */

static inline int irw_is_pascal(const char *s) {
  return s && s[0] && isupper((unsigned char)s[0]);
}

static inline int irw_is_style_attr(const char *name) {
  return name && (strcmp(name, "variant") == 0 || strcmp(name, "size") == 0 ||
                  strcmp(name, "color") == 0 || strcmp(name, "gap") == 0 ||
                  strcmp(name, "cols") == 0 || strcmp(name, "p") == 0 ||
                  strcmp(name, "bg") == 0 || strcmp(name, "shadow") == 0 ||
                  strcmp(name, "rounded") == 0 || strcmp(name, "max-w") == 0 ||
                  strcmp(name, "overflow") == 0 || strcmp(name, "w") == 0 ||
                  strcmp(name, "h") == 0 || strcmp(name, "mx") == 0 ||
                  strcmp(name, "my") == 0 || strcmp(name, "px") == 0 ||
                  strcmp(name, "py") == 0 || strcmp(name, "m") == 0 ||
                  strcmp(name, "center") == 0 || strcmp(name, "between") == 0 ||
                  strcmp(name, "bold") == 0 || strcmp(name, "muted") == 0 ||
                  strcmp(name, "sticky") == 0 || strcmp(name, "primary") == 0 ||
                  strcmp(name, "outline") == 0 || strcmp(name, "ghost") == 0);
}

static inline int irw_looks_number(const char *s) {
  if (!s || !*s) return 0;
  const char *p = s;
  if (*p == '-' || *p == '+') p++;
  int dig = 0, dot = 0;
  for (; *p; p++) {
    if (*p >= '0' && *p <= '9') dig = 1;
    else if (*p == '.' && !dot) dot = 1;
    else return 0;
  }
  return dig;
}

static inline int irw_looks_bool(const char *s) {
  return s && (strcmp(s, "true") == 0 || strcmp(s, "false") == 0);
}

static inline const char *irw_html_tag(const char *tag) {
  if (!tag) return "div";
  if (strcmp(tag, "col") == 0 || strcmp(tag, "row") == 0 ||
      strcmp(tag, "stack") == 0 || strcmp(tag, "page") == 0 ||
      strcmp(tag, "card") == 0 || strcmp(tag, "grid") == 0 ||
      strcmp(tag, "group") == 0)
    return "div";
  if (strcmp(tag, "sidebar") == 0) return "aside";
  if (strcmp(tag, "btn") == 0 || strcmp(tag, "button") == 0) return "button";
  if (strcmp(tag, "link") == 0) return "a";
  if (strcmp(tag, "fragment") == 0) return NULL;
  if (strcmp(tag, "checkbox") == 0 || strcmp(tag, "radio") == 0) return "input";
  if (strcmp(tag, "icon") == 0) return "span";
  if (strcmp(tag, "hr") == 0) return "hr";
  if (strcmp(tag, "br") == 0) return "br";
  if (strcmp(tag, "img") == 0) return "img";
  return tag;
}

/* HTML void elements — must be self-closing; no children / no </tag> */
static inline int irw_is_void_html(const char *html) {
  if (!html) return 0;
  return strcmp(html, "area") == 0 || strcmp(html, "base") == 0 ||
         strcmp(html, "br") == 0 || strcmp(html, "col") == 0 ||
         strcmp(html, "embed") == 0 || strcmp(html, "hr") == 0 ||
         strcmp(html, "img") == 0 || strcmp(html, "input") == 0 ||
         strcmp(html, "link") == 0 || strcmp(html, "meta") == 0 ||
         strcmp(html, "param") == 0 || strcmp(html, "source") == 0 ||
         strcmp(html, "track") == 0 || strcmp(html, "wbr") == 0;
}

static inline const char *irw_base_class(const char *tag) {
  if (!tag) return NULL;
  if (strcmp(tag, "col") == 0 || strcmp(tag, "stack") == 0)
    return "flex flex-col";
  if (strcmp(tag, "row") == 0) return "flex flex-row";
  if (strcmp(tag, "sidebar") == 0) return "flex flex-col";
  if (strcmp(tag, "grid") == 0) return "grid";
  if (strcmp(tag, "page") == 0) return "min-h-screen";
  if (strcmp(tag, "btn") == 0) return "btn";
  if (strcmp(tag, "card") == 0) return "card";
  return NULL;
}

/* Append Tailwind-ish classes from style attrs on IR element. */
static inline void irw_collect_classes(char *out, size_t outsz, const IrNode *el,
                                      const char *base) {
  out[0] = '\0';
  size_t n = 0;
  if (base) {
    size_t bl = strlen(base);
    if (bl + 1 < outsz) {
      memcpy(out, base, bl);
      n = bl;
      out[n] = '\0';
    }
  }
  if (!el) return;
  for (size_t i = 0; i < el->n_kids; i++) {
    IrNode *a = el->kids[i];
    if (!a || a->kind != IR_ATTR || !a->name) continue;
    const char *k = a->name;
    const char *v = a->value ? a->value : "";
    char piece[128];
    piece[0] = '\0';
    if (strcmp(k, "gap") == 0) snprintf(piece, sizeof(piece), " gap-%s", v);
    else if (strcmp(k, "p") == 0) snprintf(piece, sizeof(piece), " p-%s", v);
    else if (strcmp(k, "size") == 0) {
      if (strcmp(v, "2xl") == 0) snprintf(piece, sizeof(piece), " text-2xl");
      else if (strcmp(v, "4xl") == 0) snprintf(piece, sizeof(piece), " text-4xl");
      else if (strcmp(v, "xl") == 0) snprintf(piece, sizeof(piece), " text-xl");
      else if (strcmp(v, "lg") == 0) snprintf(piece, sizeof(piece), " text-lg");
      else if (strcmp(v, "sm") == 0) snprintf(piece, sizeof(piece), " text-sm");
      else snprintf(piece, sizeof(piece), " text-%s", v);
    } else if (strcmp(k, "bold") == 0 && (v[0] == '\0' || strcmp(v, "true") == 0))
      snprintf(piece, sizeof(piece), " font-bold");
    else if (strcmp(k, "muted") == 0 && (v[0] == '\0' || strcmp(v, "true") == 0))
      snprintf(piece, sizeof(piece), " text-gray-500");
    else if (strcmp(k, "center") == 0 && (v[0] == '\0' || strcmp(v, "true") == 0))
      snprintf(piece, sizeof(piece), " items-center justify-center");
    else if (strcmp(k, "between") == 0 && (v[0] == '\0' || strcmp(v, "true") == 0))
      snprintf(piece, sizeof(piece), " justify-between");
    else if (strcmp(k, "sticky") == 0 && (v[0] == '\0' || strcmp(v, "true") == 0))
      snprintf(piece, sizeof(piece), " sticky top-0");
    else if (strcmp(k, "bg") == 0) snprintf(piece, sizeof(piece), " bg-%s", v);
    else if (strcmp(k, "shadow") == 0)
      snprintf(piece, sizeof(piece), " shadow-%s", v);
    else if (strcmp(k, "variant") == 0)
      snprintf(piece, sizeof(piece), " btn-%s", v);
    else if (strcmp(k, "max-w") == 0)
      snprintf(piece, sizeof(piece), " max-w-%s", v);
    if (piece[0] && n + strlen(piece) + 1 < outsz) {
      memcpy(out + n, piece, strlen(piece) + 1);
      n += strlen(piece);
    }
  }
}

static inline int irw_is_decl_kind(IrKind k) {
  return k == IR_PROP || k == IR_STATE || k == IR_COMPUTED || k == IR_EFFECT ||
         k == IR_FETCH || k == IR_HOOK || k == IR_STORE || k == IR_MODULE_USE;
}

static inline int irw_hook_is(const IrNode *n, const char *kind) {
  return n && n->kind == IR_HOOK && n->name && kind && strcmp(n->name, kind) == 0;
}

#endif
