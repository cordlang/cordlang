#include "adapters/outbound/backends/esm/esm_css_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int decls_for_colors(const char *cls, char *out, size_t outn) {
  const char *v = NULL;
  char val[64];
/* arbitrary theme-var values: bg-[var(--color-primary)] */
  if (starts(cls, "bg-[var(--", &v)) {
    char tok[64];
    snprintf(tok, sizeof(tok), "%s", v);
    char *close = strchr(tok, ')');
    if (close) *close = '\0';
    snprintf(out, outn, "background-color: var(--%s)", tok);
    return 1;
  }
  if (starts(cls, "text-[var(--", &v)) {
    char tok[64];
    snprintf(tok, sizeof(tok), "%s", v);
    char *close = strchr(tok, ')');
    if (close) *close = '\0';
    snprintf(out, outn, "color: var(--%s)", tok);
    return 1;
  }

  /* typography scale */
  if (starts(cls, "text-", &v)) {
    if (strcmp(v, "xs") == 0) {
      snprintf(out, outn, "font-size: 0.75rem; line-height: 1.4");
      return 1;
    }
    if (strcmp(v, "sm") == 0) {
      snprintf(out, outn, "font-size: 0.875rem; line-height: 1.5");
      return 1;
    }
    if (strcmp(v, "base") == 0) {
      snprintf(out, outn, "font-size: 1rem; line-height: 1.6");
      return 1;
    }
    if (strcmp(v, "lg") == 0) {
      snprintf(out, outn, "font-size: 1.125rem; line-height: 1.6");
      return 1;
    }
    if (strcmp(v, "xl") == 0) {
      snprintf(out, outn, "font-size: 1.25rem; line-height: 1.5");
      return 1;
    }
    if (strcmp(v, "2xl") == 0) {
      snprintf(out, outn, "font-size: 1.5rem; line-height: 1.35");
      return 1;
    }
    if (strcmp(v, "3xl") == 0) {
      snprintf(out, outn, "font-size: 1.875rem; line-height: 1.25");
      return 1;
    }
    if (strcmp(v, "4xl") == 0) {
      snprintf(out, outn, "font-size: 2.25rem; line-height: 1.15");
      return 1;
    }
    if (strcmp(v, "5xl") == 0) {
      snprintf(out, outn, "font-size: 3rem; line-height: 1.05");
      return 1;
    }
    if (strcmp(v, "left") == 0 || strcmp(v, "center") == 0 ||
        strcmp(v, "right") == 0) {
      snprintf(out, outn, "text-align: %s", v);
      return 1;
    }
    if (strcmp(v, "muted") == 0) {
      snprintf(out, outn, "color: var(--color-muted, #57534e)");
      return 1;
    }
    if (strcmp(v, "white") == 0) {
      snprintf(out, outn, "color: #fff");
      return 1;
    }
    if (strcmp(v, "black") == 0) {
      snprintf(out, outn, "color: #000");
      return 1;
    }
    /* theme token: text-primary → var(--color-primary) */
    if (theme_is_color_token(v)) {
      char tok[64];
      if (theme_token_name(v, tok, sizeof(tok))) {
        snprintf(out, outn, "color: var(--color-%s)", tok);
        return 1;
      }
    }
    return 0;
  }

  if (starts(cls, "bg-", &v)) {
    if (strcmp(v, "white") == 0) {
      snprintf(out, outn, "background-color: #fff");
      return 1;
    }
    if (strcmp(v, "black") == 0) {
      snprintf(out, outn, "background-color: #000");
      return 1;
    }
    if (strcmp(v, "transparent") == 0) {
      snprintf(out, outn, "background-color: transparent");
      return 1;
    }
    if (theme_is_color_token(v)) {
      char tok[64];
      if (theme_token_name(v, tok, sizeof(tok))) {
        snprintf(out, outn, "background-color: var(--color-%s)", tok);
        return 1;
      }
    }
    return 0;
  }

  if (starts(cls, "border-", &v)) {
    if (theme_is_color_token(v)) {
      char tok[64];
      if (theme_token_name(v, tok, sizeof(tok))) {
        snprintf(out, outn, "border-color: var(--color-%s)", tok);
        return 1;
      }
    }
    if (strcmp(v, "gray-200") == 0) {
      snprintf(out, outn, "border-color: #e5e7eb");
      return 1;
    }
    if (strcmp(v, "gray-300") == 0) {
      snprintf(out, outn, "border-color: #d1d5db");
      return 1;
    }
    return 0;
  }
  return 0;
}
