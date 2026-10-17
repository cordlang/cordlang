#include "adapters/outbound/backends/esm/esm_css_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int decls_for_display(const char *cls, char *out, size_t outn) {
  const char *v = NULL;
  char val[64];
  if (starts(cls, "grid-cols-", &v) && all_digits(v)) {
    snprintf(out, outn, "grid-template-columns: repeat(%s, minmax(0, 1fr))", v);
    return 1;
  }
  if (starts(cls, "z-", &v) && all_digits(v)) {
    snprintf(out, outn, "z-index: %s", v);
    return 1;
  }
  if (starts(cls, "opacity-", &v) && all_digits(v)) {
    snprintf(out, outn, "opacity: %.2f", atof(v) / 100.0);
    return 1;
  }
  if (starts(cls, "top-", &v) && size_value(v, val, sizeof(val))) {
    snprintf(out, outn, "top: %s", val);
    return 1;
  }
  if (starts(cls, "leading-", &v)) {
    if (all_digits(v)) {
      snprintf(out, outn, "line-height: %.3f", atof(v) / 100.0);
      return 1;
    }
    if (strcmp(v, "tight") == 0) {
      snprintf(out, outn, "line-height: 1.2");
      return 1;
    }
    if (strcmp(v, "snug") == 0) {
      snprintf(out, outn, "line-height: 1.35");
      return 1;
    }
    if (strcmp(v, "relaxed") == 0) {
      snprintf(out, outn, "line-height: 1.75");
      return 1;
    }
    return 0;
  }
  if (starts(cls, "tracking-", &v)) {
    if (strcmp(v, "tight") == 0) {
      snprintf(out, outn, "letter-spacing: -0.02em");
      return 1;
    }
    if (strcmp(v, "wide") == 0) {
      snprintf(out, outn, "letter-spacing: 0.04em");
      return 1;
    }
    if (strcmp(v, "wider") == 0) {
      snprintf(out, outn, "letter-spacing: 0.08em");
      return 1;
    }
    return 0;
  }
  if (starts(cls, "overflow-", &v)) {
    if (strcmp(v, "x-auto") == 0) {
      snprintf(out, outn, "overflow-x: auto");
      return 1;
    }
    if (strcmp(v, "y-auto") == 0) {
      snprintf(out, outn, "overflow-y: auto");
      return 1;
    }
    snprintf(out, outn, "overflow: %s", v);
    return 1;
  }
  if (starts(cls, "shadow-", &v)) {
    if (strcmp(v, "sm") == 0) {
      snprintf(out, outn, "box-shadow: 0 1px 2px rgba(0,0,0,0.06)");
      return 1;
    }
    if (strcmp(v, "md") == 0) {
      snprintf(out, outn, "box-shadow: 0 4px 10px rgba(0,0,0,0.08)");
      return 1;
    }
    if (strcmp(v, "lg") == 0) {
      snprintf(out, outn, "box-shadow: 0 10px 25px rgba(0,0,0,0.10)");
      return 1;
    }
    if (strcmp(v, "none") == 0) {
      snprintf(out, outn, "box-shadow: none");
      return 1;
    }
    return 0;
  }
  return 0;
}
int decls_for_keywords(const char *cls, char *out, size_t outn) {
  const char *v = NULL;
  char val[64];
/* bare keywords */
  if (strcmp(cls, "flex") == 0) {
    snprintf(out, outn, "display: flex");
    return 1;
  }
  if (strcmp(cls, "grid") == 0) {
    snprintf(out, outn, "display: grid");
    return 1;
  }
  if (strcmp(cls, "flex-col") == 0) {
    snprintf(out, outn, "flex-direction: column");
    return 1;
  }
  if (strcmp(cls, "flex-row") == 0) {
    snprintf(out, outn, "flex-direction: row");
    return 1;
  }
  if (strcmp(cls, "flex-wrap") == 0) {
    snprintf(out, outn, "flex-wrap: wrap");
    return 1;
  }
  if (strcmp(cls, "flex-1") == 0) {
    snprintf(out, outn, "flex: 1 1 0%%");
    return 1;
  }
  if (strcmp(cls, "items-center") == 0) {
    snprintf(out, outn, "align-items: center");
    return 1;
  }
  if (strcmp(cls, "items-start") == 0) {
    snprintf(out, outn, "align-items: flex-start");
    return 1;
  }
  if (strcmp(cls, "items-end") == 0) {
    snprintf(out, outn, "align-items: flex-end");
    return 1;
  }
  if (strcmp(cls, "justify-center") == 0) {
    snprintf(out, outn, "justify-content: center");
    return 1;
  }
  if (strcmp(cls, "justify-between") == 0) {
    snprintf(out, outn, "justify-content: space-between");
    return 1;
  }
  if (strcmp(cls, "justify-around") == 0) {
    snprintf(out, outn, "justify-content: space-around");
    return 1;
  }
  if (strcmp(cls, "justify-evenly") == 0) {
    snprintf(out, outn, "justify-content: space-evenly");
    return 1;
  }
  if (strcmp(cls, "font-bold") == 0) {
    snprintf(out, outn, "font-weight: 700");
    return 1;
  }
  if (strcmp(cls, "font-medium") == 0) {
    snprintf(out, outn, "font-weight: 500");
    return 1;
  }
  if (strcmp(cls, "sticky") == 0) {
    snprintf(out, outn, "position: sticky");
    return 1;
  }
  if (strcmp(cls, "relative") == 0) {
    snprintf(out, outn, "position: relative");
    return 1;
  }
  if (strcmp(cls, "absolute") == 0) {
    snprintf(out, outn, "position: absolute");
    return 1;
  }
  if (strcmp(cls, "fixed") == 0) {
    snprintf(out, outn, "position: fixed");
    return 1;
  }
  if (strcmp(cls, "border") == 0) {
    snprintf(out, outn, "border-width: 1px; border-style: solid");
    return 1;
  }
  if (strcmp(cls, "rounded") == 0) {
    snprintf(out, outn, "border-radius: var(--radius, 0.5rem)");
    return 1;
  }
  if (strcmp(cls, "type-display") == 0) {
    snprintf(out, outn,
             "font-family: var(--font-display, var(--font-sans, system-ui)); "
             "font-size: clamp(2rem, 5vw, 3.25rem); line-height: 1.05; "
             "letter-spacing: -0.03em");
    return 1;
  }
  if (strcmp(cls, "type-title") == 0) {
    snprintf(out, outn,
             "font-family: var(--font-display, var(--font-sans, system-ui)); "
             "font-size: 1.5rem; line-height: 1.25; letter-spacing: -0.02em");
    return 1;
  }
  if (strcmp(cls, "type-body") == 0) {
    snprintf(out, outn, "font-size: 1rem; line-height: 1.65");
    return 1;
  }
  if (strcmp(cls, "type-caption") == 0) {
    snprintf(out, outn,
             "font-size: 0.75rem; letter-spacing: 0.1em; text-transform: uppercase");
    return 1;
  }
  if (strcmp(cls, "type-code") == 0) {
    snprintf(out, outn,
             "font-family: var(--font-mono, ui-monospace, monospace); "
             "font-size: 0.9rem");
    return 1;
  }
  return 0;
}
