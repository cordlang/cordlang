#include "adapters/inbound/wasm_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void sb_grow(char **buf, size_t *len, size_t *cap, size_t need) {
  if (*len + need + 1 <= *cap) return;
  size_t ncap = *cap ? *cap : 256;
  while (*len + need + 1 > ncap) ncap *= 2;
  char *nbuf = realloc(*buf, ncap);
  if (!nbuf) return;
  *buf = nbuf;
  *cap = ncap;
}
static void sb_putc(char **buf, size_t *len, size_t *cap, char c) {
  sb_grow(buf, len, cap, 1);
  if (!*buf) return;
  (*buf)[(*len)++] = c;
  (*buf)[*len] = '\0';
}
static void sb_puts(char **buf, size_t *len, size_t *cap, const char *s) {
  if (!s) return;
  size_t n = strlen(s);
  sb_grow(buf, len, cap, n);
  if (!*buf) return;
  memcpy(*buf + *len, s, n);
  *len += n;
  (*buf)[*len] = '\0';
}
static void sb_put_json_str(char **buf, size_t *len, size_t *cap, const char *s) {
  sb_putc(buf, len, cap, '"');
  if (!s) {
    sb_putc(buf, len, cap, '"');
    return;
  }
  for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
    if (*p == '"' || *p == '\\') {
      sb_putc(buf, len, cap, '\\');
      sb_putc(buf, len, cap, (char)*p);
    } else if (*p == '\n') {
      sb_puts(buf, len, cap, "\\n");
    } else if (*p == '\r') {
      sb_puts(buf, len, cap, "\\r");
    } else if (*p == '\t') {
      sb_puts(buf, len, cap, "\\t");
    } else if (*p < 0x20) {
      char hex[8];
      snprintf(hex, sizeof(hex), "\\u%04x", *p);
      sb_puts(buf, len, cap, hex);
    } else {
      sb_putc(buf, len, cap, (char)*p);
    }
  }
  sb_putc(buf, len, cap, '"');
}
static char *wasm_build_result_json(int ok, const char *backend, const char *code,
                               const DiagList *diags, const char *parse_error,
                               int parse_line, int parse_col) {
  char *buf = NULL;
  size_t len = 0, cap = 0;
  sb_puts(&buf, &len, &cap, "{\"ok\":");
  sb_puts(&buf, &len, &cap, ok ? "true" : "false");
  sb_puts(&buf, &len, &cap, ",\"backend\":");
  sb_put_json_str(&buf, &len, &cap, backend ? backend : "");
  sb_puts(&buf, &len, &cap, ",\"code\":");
  sb_put_json_str(&buf, &len, &cap, code ? code : "");
  sb_puts(&buf, &len, &cap, ",\"diagnostics\":[");

  int first = 1;
  if (parse_error && *parse_error) {
    first = 0;
    sb_puts(&buf, &len, &cap, "{\"level\":\"error\",\"message\":");
    sb_put_json_str(&buf, &len, &cap, parse_error);
    sb_puts(&buf, &len, &cap, ",\"line\":");
    char num[32];
    snprintf(num, sizeof(num), "%d", parse_line > 0 ? parse_line : 1);
    sb_puts(&buf, &len, &cap, num);
    sb_puts(&buf, &len, &cap, ",\"col\":");
    snprintf(num, sizeof(num), "%d", parse_col > 0 ? parse_col : 1);
    sb_puts(&buf, &len, &cap, num);
    sb_puts(&buf, &len, &cap, "}");
  }

  if (diags) {
    for (size_t i = 0; i < diags->len; i++) {
      const Diagnostic *d = &diags->items[i];
      if (!first) sb_putc(&buf, &len, &cap, ',');
      first = 0;
      sb_puts(&buf, &len, &cap, "{\"level\":");
      sb_put_json_str(&buf, &len, &cap,
                      d->level == DIAG_ERROR ? "error" : "warning");
      sb_puts(&buf, &len, &cap, ",\"message\":");
      sb_put_json_str(&buf, &len, &cap, d->message ? d->message : "");
      if (d->file && *d->file) {
        sb_puts(&buf, &len, &cap, ",\"file\":");
        sb_put_json_str(&buf, &len, &cap, d->file);
      }
      if (d->code && *d->code) {
        sb_puts(&buf, &len, &cap, ",\"code\":");
        sb_put_json_str(&buf, &len, &cap, d->code);
      }
      if (d->hint && *d->hint) {
        sb_puts(&buf, &len, &cap, ",\"hint\":");
        sb_put_json_str(&buf, &len, &cap, d->hint);
      }
      sb_puts(&buf, &len, &cap, ",\"line\":");
      char num[32];
      snprintf(num, sizeof(num), "%d", d->line > 0 ? d->line : 1);
      sb_puts(&buf, &len, &cap, num);
      sb_puts(&buf, &len, &cap, ",\"col\":");
      snprintf(num, sizeof(num), "%d", d->col > 0 ? d->col : 1);
      sb_puts(&buf, &len, &cap, num);
      sb_puts(&buf, &len, &cap, "}");
    }
  }

  sb_puts(&buf, &len, &cap, "]}");
  return buf ? buf : strdup("{\"ok\":false,\"backend\":\"\",\"code\":\"\",\"diagnostics\":[]}");
}
