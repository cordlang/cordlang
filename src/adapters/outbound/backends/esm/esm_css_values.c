#include "adapters/outbound/backends/esm/esm_css_internal.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void fmt_rem(char *out, size_t n, double px) {
  char tmp[64];
  snprintf(tmp, sizeof(tmp), "%.4f", px / 16.0);
  /* trim trailing zeros / dot */
  size_t l = strlen(tmp);
  while (l > 0 && tmp[l - 1] == '0') tmp[--l] = '\0';
  if (l > 0 && tmp[l - 1] == '.') tmp[--l] = '\0';
  snprintf(out, n, "%srem", tmp[0] ? tmp : "0");
}

int all_digits(const char *s) {
  if (!s || !*s) return 0;
  for (const char *p = s; *p; p++)
    if (!isdigit((unsigned char)*p)) return 0;
  return 1;
}

int size_value(const char *v, char *out, size_t n) {
  if (!v || !*v) return 0;
  if (all_digits(v)) {
    fmt_rem(out, n, atof(v));
    return 1;
  }
  if (strcmp(v, "full") == 0) {
    snprintf(out, n, "100%%");
    return 1;
  }
  if (strcmp(v, "screen") == 0) {
    snprintf(out, n, "100vh");
    return 1;
  }
  if (strcmp(v, "auto") == 0) {
    snprintf(out, n, "auto");
    return 1;
  }
  if (strcmp(v, "px") == 0) {
    snprintf(out, n, "1px");
    return 1;
  }
  if (strcmp(v, "min") == 0) {
    snprintf(out, n, "min-content");
    return 1;
  }
  if (strcmp(v, "max") == 0) {
    snprintf(out, n, "max-content");
    return 1;
  }
  if (strcmp(v, "fit") == 0) {
    snprintf(out, n, "fit-content");
    return 1;
  }
  if (strcmp(v, "0") == 0) {
    snprintf(out, n, "0");
    return 1;
  }
  return 0;
}

void sel_escape(const char *cls, char *out, size_t n) {
  size_t o = 0;
  for (const char *p = cls; *p && o + 2 < n; p++) {
    if (*p == '[' || *p == ']' || *p == '(' || *p == ')' || *p == '.' ||
        *p == ':' || *p == '/' || *p == '%' || *p == '#' || *p == ',') {
      out[o++] = '\\';
    }
    out[o++] = *p;
  }
  out[o] = '\0';
}

int starts(const char *s, const char *pfx, const char **rest) {
  size_t n = strlen(pfx);
  if (strncmp(s, pfx, n) != 0) return 0;
  *rest = s + n;
  return 1;
}
