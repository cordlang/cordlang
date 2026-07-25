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
  if (!name) return 0;
  /* Responsive prefixes: sm:gap, md:p, lg:cols */
  if ((strncmp(name, "sm:", 3) == 0 || strncmp(name, "md:", 3) == 0 ||
       strncmp(name, "lg:", 3) == 0) &&
      name[3])
    return irw_is_style_attr(name + 3);
  return (strcmp(name, "variant") == 0 || strcmp(name, "size") == 0 ||
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
          strcmp(name, "outline") == 0 || strcmp(name, "ghost") == 0 ||
          strcmp(name, "border") == 0 || strcmp(name, "min-h") == 0 ||
          strcmp(name, "flex-1") == 0 || strcmp(name, "font-mono") == 0 ||
          strcmp(name, "leading") == 0 || strcmp(name, "tracking") == 0 ||
          strcmp(name, "elevate") == 0 || strcmp(name, "density") == 0);
}

static inline int irw_is_type_scale(const char *v) {
  return v && (strcmp(v, "display") == 0 || strcmp(v, "title") == 0 ||
               strcmp(v, "body") == 0 || strcmp(v, "caption") == 0 ||
               strcmp(v, "code") == 0);
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
  if (strcmp(tag, "section") == 0) return "section";
  if (strcmp(tag, "sidebar") == 0) return "aside";
  if (strcmp(tag, "btn") == 0 || strcmp(tag, "button") == 0) return "button";
  if (strcmp(tag, "link") == 0) return "a";
  if (strcmp(tag, "fragment") == 0) return NULL;
  if (strcmp(tag, "checkbox") == 0 || strcmp(tag, "radio") == 0) return "input";
  /* Capability tags: bridge names for React/Svelte; HTML backend maps separately. */
  if (strcmp(tag, "icon") == 0) return "CordIcon";
  if (strcmp(tag, "motion") == 0 || strcmp(tag, "Motion") == 0) return "CordMotion";
  if (strcmp(tag, "chart") == 0 || strcmp(tag, "Chart") == 0) return "CordChart";
  if (strcmp(tag, "hr") == 0) return "hr";
  if (strcmp(tag, "br") == 0) return "br";
  if (strcmp(tag, "img") == 0) return "img";
  return tag;
}

static inline int irw_is_cord_capability_tag(const char *tag) {
  return tag && (strcmp(tag, "icon") == 0 || strcmp(tag, "motion") == 0 ||
                 strcmp(tag, "Motion") == 0 || strcmp(tag, "chart") == 0 ||
                 strcmp(tag, "Chart") == 0);
}

static inline const char *irw_cord_bridge_name(const char *tag) {
  if (!tag) return NULL;
  if (strcmp(tag, "icon") == 0) return "CordIcon";
  if (strcmp(tag, "motion") == 0 || strcmp(tag, "Motion") == 0) return "CordMotion";
  if (strcmp(tag, "chart") == 0 || strcmp(tag, "Chart") == 0) return "CordChart";
  return NULL;
}

/* HTML void elements — self-closing; no children / no </tag> */
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
  if (strcmp(tag, "section") == 0) return "cord-section";
  if (strcmp(tag, "btn") == 0) return "btn";
  if (strcmp(tag, "card") == 0) return "card";
  return NULL;
}

/* Map one style key/value (no responsive prefix) into a class piece. */
static inline void irw_style_piece(char *piece, size_t psz, const char *k,
                                  const char *v, int has_between) {
  piece[0] = '\0';
  if (!k) return;
  if (!v) v = "";
  if (strcmp(k, "gap") == 0) snprintf(piece, psz, " gap-%s", v);
  else if (strcmp(k, "p") == 0) snprintf(piece, psz, " p-%s", v);
  else if (strcmp(k, "w") == 0) snprintf(piece, psz, " w-%s", v);
  else if (strcmp(k, "h") == 0) snprintf(piece, psz, " h-%s", v);
  else if (strcmp(k, "min-h") == 0) snprintf(piece, psz, " min-h-%s", v);
  else if (strcmp(k, "size") == 0) {
    if (strcmp(v, "2xl") == 0) snprintf(piece, psz, " text-2xl");
    else if (strcmp(v, "4xl") == 0) snprintf(piece, psz, " text-4xl");
    else if (strcmp(v, "xl") == 0) snprintf(piece, psz, " text-xl");
    else if (strcmp(v, "lg") == 0) snprintf(piece, psz, " text-lg");
    else if (strcmp(v, "sm") == 0) snprintf(piece, psz, " text-sm");
    else snprintf(piece, psz, " text-%s", v);
  } else if (strcmp(k, "type") == 0 && irw_is_type_scale(v))
    snprintf(piece, psz, " type-%s", v);
  else if (strcmp(k, "elevate") == 0)
    snprintf(piece, psz, " elevate-%s", v);
  else if (strcmp(k, "density") == 0)
    snprintf(piece, psz, " density-%s", v);
  else if (strcmp(k, "leading") == 0)
    snprintf(piece, psz, " leading-%s", v);
  else if (strcmp(k, "tracking") == 0)
    snprintf(piece, psz, " tracking-%s", v);
  else if (strcmp(k, "bold") == 0 && (v[0] == '\0' || strcmp(v, "true") == 0))
    snprintf(piece, psz, " font-bold");
  else if (strcmp(k, "muted") == 0 && (v[0] == '\0' || strcmp(v, "true") == 0))
    snprintf(piece, psz, " text-muted");
  else if (strcmp(k, "font-mono") == 0 && (v[0] == '\0' || strcmp(v, "true") == 0))
    snprintf(piece, psz, " font-mono");
  else if (strcmp(k, "flex-1") == 0 && (v[0] == '\0' || strcmp(v, "true") == 0))
    snprintf(piece, psz, " flex-1");
  else if (strcmp(k, "border") == 0 && (v[0] == '\0' || strcmp(v, "true") == 0))
    snprintf(piece, psz, " border border-gray-200");
  else if (strcmp(k, "border") == 0)
    snprintf(piece, psz, " border border-%s", v);
  else if (strcmp(k, "center") == 0 && (v[0] == '\0' || strcmp(v, "true") == 0))
    snprintf(piece, psz,
             has_between ? " flex items-center" : " flex items-center justify-center");
  else if (strcmp(k, "between") == 0 && (v[0] == '\0' || strcmp(v, "true") == 0))
    snprintf(piece, psz, " flex justify-between");
  else if (strcmp(k, "sticky") == 0 && (v[0] == '\0' || strcmp(v, "true") == 0))
    snprintf(piece, psz, " sticky top-0");
  else if (strcmp(k, "bg") == 0) snprintf(piece, psz, " bg-%s", v);
  else if (strcmp(k, "shadow") == 0)
    snprintf(piece, psz, " shadow-%s", v);
  else if (strcmp(k, "variant") == 0)
    snprintf(piece, psz, " btn-%s", v);
  else if (strcmp(k, "max-w") == 0)
    snprintf(piece, psz, " max-w-%s", v);
  else if (strcmp(k, "rounded") == 0)
    snprintf(piece, psz, " rounded-%s", v);
  else if (strcmp(k, "mx") == 0) snprintf(piece, psz, " mx-%s", v);
  else if (strcmp(k, "my") == 0) snprintf(piece, psz, " my-%s", v);
  else if (strcmp(k, "px") == 0) snprintf(piece, psz, " px-%s", v);
  else if (strcmp(k, "py") == 0) snprintf(piece, psz, " py-%s", v);
  else if (strcmp(k, "m") == 0) snprintf(piece, psz, " m-%s", v);
  else if (strcmp(k, "cols") == 0) snprintf(piece, psz, " grid-cols-%s", v);
  else if (strcmp(k, "overflow") == 0) snprintf(piece, psz, " overflow-%s", v);
  else if (strcmp(k, "color") == 0) snprintf(piece, psz, " text-%s", v);
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
  int has_between = 0;
  for (size_t i = 0; i < el->n_kids; i++) {
    IrNode *a = el->kids[i];
    if (!a || a->kind != IR_ATTR || !a->name) continue;
    if (strcmp(a->name, "between") == 0) has_between = 1;
  }
  for (size_t i = 0; i < el->n_kids; i++) {
    IrNode *a = el->kids[i];
    if (!a || a->kind != IR_ATTR || !a->name) continue;
    const char *k = a->name;
    const char *v = a->value ? a->value : "";
    char piece[160];
    piece[0] = '\0';
    /* type=display|title|… even though type is also a DOM attr */
    if (strcmp(k, "type") == 0 && irw_is_type_scale(v)) {
      snprintf(piece, sizeof(piece), " type-%s", v);
    } else if (strncmp(k, "sm:", 3) == 0 || strncmp(k, "md:", 3) == 0 ||
               strncmp(k, "lg:", 3) == 0) {
      char inner[128];
      irw_style_piece(inner, sizeof(inner), k + 3, v, has_between);
      if (inner[0]) {
        /* strip leading space from inner, prefix breakpoint */
        const char *cls = inner;
        while (*cls == ' ') cls++;
        snprintf(piece, sizeof(piece), " %.2s:%s", k, cls);
      }
    } else if (irw_is_style_attr(k)) {
      irw_style_piece(piece, sizeof(piece), k, v, has_between);
    }
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

/* Truthy Cord attr: missing/empty/"true" (shared by React/Solid emit). */
static inline int irw_attr_truthy(const char *v) {
  return !v || !*v || strcmp(v, "true") == 0;
}

static inline int irw_route_has_lazy(const IrNode *route) {
  if (!route) return 0;
  for (size_t i = 0; i < route->n_kids; i++) {
    const IrNode *c = route->kids[i];
    if (c && c->kind == IR_ATTR && c->name && strcmp(c->name, "lazy") == 0 &&
        irw_attr_truthy(c->value))
      return 1;
  }
  return 0;
}

#endif
