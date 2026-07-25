/*
 * Shared Cord attr → utility-class lowering (see cord_class.h).
 *
 * Behaviour is the React backend's original mapping, moved here verbatim so
 * the React scaffold and the native ESM preview cannot drift.
 */
#include "adapters/outbound/backends/cord_class.h"
#include "adapters/outbound/backends/theme_css.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

int cord_looks_like_number(const char *s) {
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

int cord_looks_like_bool(const char *s) {
  return s && (strcmp(s, "true") == 0 || strcmp(s, "false") == 0);
}

int cord_is_dom_a11y_or_data_attr(const char *name) {
  if (!name || !*name) return 0;
  if (strcmp(name, "role") == 0) return 1;
  if (strncmp(name, "aria-", 5) == 0) return 1;
  if (strncmp(name, "data-", 5) == 0) return 1;
  return 0;
}

int cord_attr_is_true(const char *v) {
  return !v || !*v || strcmp(v, "true") == 0;
}

int cord_is_style_attr_name(const char *name) {
  if (!name) return 0;
  if ((strncmp(name, "sm:", 3) == 0 || strncmp(name, "md:", 3) == 0 ||
       strncmp(name, "lg:", 3) == 0) &&
      name[3])
    return cord_is_style_attr_name(name + 3);
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

int cord_is_style_bool_name(const char *name) {
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

const char *cord_tag_base_class(const char *tag) {
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

const char *cord_html_tag_for(const char *tag) {
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

void cord_collect_classes(char *classes, size_t classes_sz, IrNode *node,
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

    if (cord_attr_is_true(v)) {
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
