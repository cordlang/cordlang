#ifndef CORDLANG_DOMAIN_KNOWN_ATTRS_H
#define CORDLANG_DOMAIN_KNOWN_ATTRS_H

#include <ctype.h>
#include <string.h>

/* Keep in sync with docs/schema/attrs.json */

static inline int cord_is_prop_type(const char *t) {
  return t && (strcmp(t, "string") == 0 || strcmp(t, "number") == 0 ||
               strcmp(t, "boolean") == 0 || strcmp(t, "any") == 0);
}

static inline int cord_is_forbidden_jsx_attr(const char *name) {
  return name &&
         (strcmp(name, "className") == 0 || strcmp(name, "onClick") == 0 ||
          strcmp(name, "onChange") == 0 || strcmp(name, "onSubmit") == 0 ||
          strcmp(name, "onInput") == 0 || strcmp(name, "htmlFor") == 0 ||
          strcmp(name, "defaultValue") == 0 ||
          strcmp(name, "dangerouslySetInnerHTML") == 0);
}

/* Fix hint for agents when a forbidden JSX attr is used. NULL if unknown. */
static inline const char *cord_jsx_attr_hint(const char *name) {
  if (!name) return NULL;
  if (strcmp(name, "className") == 0)
    return "use class=... or a style attr (gap/p/...)";
  if (strcmp(name, "onClick") == 0) return "use @click=...";
  if (strcmp(name, "onChange") == 0) return "use @change=...";
  if (strcmp(name, "onInput") == 0) return "use @input=...";
  if (strcmp(name, "onSubmit") == 0) return "use @submit= or form action=";
  if (strcmp(name, "htmlFor") == 0) return "use for=";
  if (strcmp(name, "defaultValue") == 0) return "use value= / bind=";
  if (strcmp(name, "dangerouslySetInnerHTML") == 0)
    return "not supported in .cord";
  return NULL;
}

/* Replacement attr name for LSP codeAction (NULL if no simple rename). */
static inline const char *cord_jsx_attr_replace(const char *name) {
  if (!name) return NULL;
  if (strcmp(name, "className") == 0) return "class";
  if (strcmp(name, "onClick") == 0) return "@click";
  if (strcmp(name, "onChange") == 0) return "@change";
  if (strcmp(name, "onInput") == 0) return "@input";
  if (strcmp(name, "onSubmit") == 0) return "@submit";
  if (strcmp(name, "htmlFor") == 0) return "for";
  return NULL;
}

static inline int cord_is_purpose_vocab(const char *v) {
  return v && (strcmp(v, "navigation") == 0 || strcmp(v, "content") == 0 ||
               strcmp(v, "action") == 0 || strcmp(v, "form") == 0 ||
               strcmp(v, "status") == 0 || strcmp(v, "decoration") == 0 ||
               strcmp(v, "landmark") == 0);
}

static inline int cord_is_importance_vocab(const char *v) {
  return v && (strcmp(v, "primary") == 0 || strcmp(v, "secondary") == 0 ||
               strcmp(v, "tertiary") == 0 || strcmp(v, "optional") == 0 ||
               strcmp(v, "critical") == 0);
}

static inline int cord_is_style_attr(const char *name) {
  return name &&
         (strcmp(name, "variant") == 0 || strcmp(name, "size") == 0 ||
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
          strcmp(name, "flex-1") == 0 || strcmp(name, "font-mono") == 0);
}

static inline int cord_is_dom_attr(const char *name) {
  return name &&
         (strcmp(name, "id") == 0 || strcmp(name, "name") == 0 ||
          strcmp(name, "type") == 0 || strcmp(name, "value") == 0 ||
          strcmp(name, "placeholder") == 0 || strcmp(name, "href") == 0 ||
          strcmp(name, "to") == 0 || strcmp(name, "src") == 0 ||
          strcmp(name, "alt") == 0 || strcmp(name, "target") == 0 ||
          strcmp(name, "rel") == 0 || strcmp(name, "role") == 0 ||
          strcmp(name, "title") == 0 || strcmp(name, "for") == 0 ||
          strcmp(name, "action") == 0 || strcmp(name, "method") == 0 ||
          strcmp(name, "disabled") == 0 || strcmp(name, "checked") == 0 ||
          strcmp(name, "readonly") == 0 || strcmp(name, "required") == 0 ||
          strcmp(name, "multiple") == 0 || strcmp(name, "maxlength") == 0 ||
          strcmp(name, "min") == 0 || strcmp(name, "max") == 0 ||
          strcmp(name, "step") == 0 || strcmp(name, "pattern") == 0 ||
          strcmp(name, "autocomplete") == 0 || strcmp(name, "tabindex") == 0 ||
          strcmp(name, "class") == 0 || strcmp(name, "style") == 0 ||
          strcmp(name, "bind") == 0 || strcmp(name, "key") == 0 ||
          strcmp(name, "use") == 0 || strcmp(name, "transition") == 0 ||
          strcmp(name, "layout") == 0 || strcmp(name, "lazy") == 0 ||
          strcmp(name, "fallback") == 0 || strcmp(name, "context") == 0 ||
          strcmp(name, "init") == 0 || strcmp(name, "pending") == 0 ||
          strcmp(name, "deps") == 0 || strcmp(name, "cleanup") == 0 ||
          strcmp(name, "getSnapshot") == 0 ||
          strcmp(name, "getServerSnapshot") == 0 || strcmp(name, "then") == 0 ||
          strcmp(name, "catch") == 0 || strcmp(name, "as") == 0 ||
          strcmp(name, "when") == 0 || strcmp(name, "of") == 0 ||
          /* input type shorthands: input text / input email */
          strcmp(name, "text") == 0 || strcmp(name, "email") == 0 ||
          strcmp(name, "password") == 0 || strcmp(name, "search") == 0 ||
          strcmp(name, "tel") == 0 || strcmp(name, "url") == 0 ||
          strcmp(name, "date") == 0 || strcmp(name, "time") == 0 ||
          strcmp(name, "datetime-local") == 0 || strcmp(name, "file") == 0 ||
          strcmp(name, "submit") == 0 || strcmp(name, "reset") == 0 ||
          strcmp(name, "button") == 0 || strcmp(name, "number") == 0 ||
          strcmp(name, "range") == 0 || strcmp(name, "hidden") == 0 ||
          /* Semantic metadata (AI / a11y intent) → data-purpose / data-importance */
          strcmp(name, "purpose") == 0 || strcmp(name, "importance") == 0);
}

static inline int cord_is_builtin_tag(const char *tag) {
  if (!tag || !*tag) return 0;
  if (isupper((unsigned char)tag[0])) return 0; /* component */
  return strcmp(tag, "col") == 0 || strcmp(tag, "row") == 0 ||
         strcmp(tag, "stack") == 0 || strcmp(tag, "page") == 0 ||
         strcmp(tag, "card") == 0 || strcmp(tag, "grid") == 0 ||
         strcmp(tag, "group") == 0 || strcmp(tag, "fragment") == 0 ||
         strcmp(tag, "btn") == 0 || strcmp(tag, "button") == 0 ||
         strcmp(tag, "link") == 0 || strcmp(tag, "input") == 0 ||
         strcmp(tag, "textarea") == 0 || strcmp(tag, "select") == 0 ||
         strcmp(tag, "checkbox") == 0 || strcmp(tag, "radio") == 0 ||
         strcmp(tag, "form") == 0 || strcmp(tag, "label") == 0 ||
         strcmp(tag, "img") == 0 || strcmp(tag, "icon") == 0 ||
         strcmp(tag, "span") == 0 || strcmp(tag, "p") == 0 ||
         strcmp(tag, "h1") == 0 || strcmp(tag, "h2") == 0 ||
         strcmp(tag, "h3") == 0 || strcmp(tag, "h4") == 0 ||
         strcmp(tag, "h5") == 0 || strcmp(tag, "h6") == 0 ||
         strcmp(tag, "ul") == 0 || strcmp(tag, "ol") == 0 ||
         strcmp(tag, "li") == 0 || strcmp(tag, "nav") == 0 ||
         strcmp(tag, "header") == 0 || strcmp(tag, "footer") == 0 ||
         strcmp(tag, "main") == 0 || strcmp(tag, "aside") == 0 ||
         strcmp(tag, "section") == 0 || strcmp(tag, "article") == 0 ||
         strcmp(tag, "div") == 0 || strcmp(tag, "a") == 0 ||
         strcmp(tag, "table") == 0 || strcmp(tag, "thead") == 0 ||
         strcmp(tag, "tbody") == 0 || strcmp(tag, "tr") == 0 ||
         strcmp(tag, "td") == 0 || strcmp(tag, "th") == 0 ||
         strcmp(tag, "code") == 0 || strcmp(tag, "pre") == 0 ||
         strcmp(tag, "hr") == 0 || strcmp(tag, "br") == 0 ||
         strcmp(tag, "slot") == 0 || strcmp(tag, "provide") == 0 ||
         strcmp(tag, "portal") == 0 || strcmp(tag, "suspense") == 0 ||
         strcmp(tag, "loading") == 0 || strcmp(tag, "errorBoundary") == 0;
}

static inline int cord_is_known_attr(const char *name) {
  if (!name || !*name) return 0;
  if (name[0] == '@') return 1;
  if (strncmp(name, "aria-", 5) == 0 || strncmp(name, "data-", 5) == 0)
    return 1;
  return cord_is_style_attr(name) || cord_is_dom_attr(name);
}

#endif
