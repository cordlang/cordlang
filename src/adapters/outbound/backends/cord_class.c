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
