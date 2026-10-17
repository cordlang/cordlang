#include "adapters/outbound/backends/esm/esm_css_internal.h"
#include <stdio.h>
#include <string.h>

int decls_for_spacing(const char *cls, char *out, size_t outn) {
  const char *v = NULL;
  char val[64];
/* spacing */
  if (starts(cls, "gap-", &v) && size_value(v, val, sizeof(val))) {
    snprintf(out, outn, "gap: %s", val);
    return 1;
  }
  if (starts(cls, "px-", &v) && size_value(v, val, sizeof(val))) {
    snprintf(out, outn, "padding-left: %s; padding-right: %s", val, val);
    return 1;
  }
  if (starts(cls, "py-", &v) && size_value(v, val, sizeof(val))) {
    snprintf(out, outn, "padding-top: %s; padding-bottom: %s", val, val);
    return 1;
  }
  if (starts(cls, "pt-", &v) && size_value(v, val, sizeof(val))) {
    snprintf(out, outn, "padding-top: %s", val);
    return 1;
  }
  if (starts(cls, "pb-", &v) && size_value(v, val, sizeof(val))) {
    snprintf(out, outn, "padding-bottom: %s", val);
    return 1;
  }
  if (starts(cls, "p-", &v) && size_value(v, val, sizeof(val))) {
    snprintf(out, outn, "padding: %s", val);
    return 1;
  }
  if (starts(cls, "mx-", &v)) {
    if (strcmp(v, "auto") == 0) {
      snprintf(out, outn, "margin-left: auto; margin-right: auto");
      return 1;
    }
    if (size_value(v, val, sizeof(val))) {
      snprintf(out, outn, "margin-left: %s; margin-right: %s", val, val);
      return 1;
    }
    return 0;
  }
  if (starts(cls, "my-", &v) && size_value(v, val, sizeof(val))) {
    snprintf(out, outn, "margin-top: %s; margin-bottom: %s", val, val);
    return 1;
  }
  if (starts(cls, "mt-", &v) && size_value(v, val, sizeof(val))) {
    snprintf(out, outn, "margin-top: %s", val);
    return 1;
  }
  if (starts(cls, "mb-", &v) && size_value(v, val, sizeof(val))) {
    snprintf(out, outn, "margin-bottom: %s", val);
    return 1;
  }
  if (starts(cls, "m-", &v) && size_value(v, val, sizeof(val))) {
    snprintf(out, outn, "margin: %s", val);
    return 1;
  }
  return 0;
}
int decls_for_sizing(const char *cls, char *out, size_t outn) {
  const char *v = NULL;
  char val[64];
/* sizing */
  if (starts(cls, "max-w-", &v) && size_value(v, val, sizeof(val))) {
    snprintf(out, outn, "max-width: %s", val);
    return 1;
  }
  if (starts(cls, "min-h-", &v)) {
    if (strcmp(v, "screen") == 0) {
      snprintf(out, outn, "min-height: 100vh");
      return 1;
    }
    if (size_value(v, val, sizeof(val))) {
      snprintf(out, outn, "min-height: %s", val);
      return 1;
    }
    return 0;
  }
  if (starts(cls, "w-", &v)) {
    if (strcmp(v, "screen") == 0) {
      snprintf(out, outn, "width: 100vw");
      return 1;
    }
    if (size_value(v, val, sizeof(val))) {
      snprintf(out, outn, "width: %s", val);
      return 1;
    }
    return 0;
  }
  if (starts(cls, "h-", &v)) {
    if (strcmp(v, "screen") == 0) {
      snprintf(out, outn, "height: 100vh");
      return 1;
    }
    if (size_value(v, val, sizeof(val))) {
      snprintf(out, outn, "height: %s", val);
      return 1;
    }
    return 0;
  }
  return 0;
}
int decls_for_radius(const char *cls, char *out, size_t outn) {
  const char *v = NULL;
/* radius: rounded-12 (px) and the keyword scale */
  if (starts(cls, "rounded-", &v)) {
    if (all_digits(v)) {
      snprintf(out, outn, "border-radius: %spx", v);
      return 1;
    }
    if (strcmp(v, "full") == 0) {
      snprintf(out, outn, "border-radius: 9999px");
      return 1;
    }
    if (strcmp(v, "none") == 0) {
      snprintf(out, outn, "border-radius: 0");
      return 1;
    }
    if (strcmp(v, "sm") == 0) {
      snprintf(out, outn, "border-radius: 0.25rem");
      return 1;
    }
    if (strcmp(v, "md") == 0) {
      snprintf(out, outn, "border-radius: 0.5rem");
      return 1;
    }
    if (strcmp(v, "lg") == 0) {
      snprintf(out, outn, "border-radius: 0.75rem");
      return 1;
    }
    if (strcmp(v, "xl") == 0) {
      snprintf(out, outn, "border-radius: 1rem");
      return 1;
    }
    if (strcmp(v, "2xl") == 0) {
      snprintf(out, outn, "border-radius: 1.25rem");
      return 1;
    }
    return 0;
  }
  return 0;
}
