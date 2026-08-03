#include "application/compile_service.h"
#include "application/check_service.h"
#include "application/ports/backend_port.h"
#include "application/ports/compiler_port.h"
#include "domain/diag.h"
#include "domain/ir.h"
#include "domain/version.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define CORD_WASM_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define CORD_WASM_EXPORT
#endif

/* Playground / WASM JS bridge.
 *
 * Exports (emscripten -sEXPORTED_FUNCTIONS):
 *   _cordlang_version
 *   _cordlang_compile   — malloc'd JSON; free with _cordlang_free
 *   _cordlang_free
 *
 * JSON shape:
 *   { "ok": true|false, "backend": "...", "code": "...", "diagnostics": [...] }
 */

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

static char *build_result_json(int ok, const char *backend, const char *code,
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
      if (d->code && *d->code) {
        sb_puts(&buf, &len, &cap, ",\"code\":");
        sb_put_json_str(&buf, &len, &cap, d->code);
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

CORD_WASM_EXPORT const char *cordlang_version(void) { return CORDLANG_VERSION; }

CORD_WASM_EXPORT void cordlang_free(char *p) { free(p); }

/*
 * Compile in-memory .cord source.
 * backend: react | svelte | vue | html | esm | email | ir
 * Returns malloc'd JSON (see file header). Free with cordlang_free.
 */
CORD_WASM_EXPORT char *cordlang_compile(const char *source, const char *backend) {
  if (!source) source = "";
  if (!backend || !*backend) backend = "react";

  backend_register_all();

  size_t source_len = strlen(source);
  CompileResult parsed = compiler_parse_source(source, source_len);
  if (!parsed.ok || !parsed.ast) {
    char *json = build_result_json(0, backend, NULL, NULL,
                                   parsed.error ? parsed.error : "parse error",
                                   parsed.error_line, parsed.error_col);
    compiler_result_free(&parsed);
    return json;
  }

  DiagList diags;
  diag_list_init(&diags);
  check_service_run_source("<playground>", source, source_len, &diags);
  int has_errors = diag_error_count(&diags) > 0;

  char *code = NULL;
  if (!has_errors) {
    code = compile_service_source(source, source_len, backend, "<playground>");
    if (!code) {
      diag_emit(&diags, DIAG_ERROR, "<playground>", 1, 1,
                "codegen failed for backend '%s'", backend);
      has_errors = 1;
    }
  }

  char *json = build_result_json(!has_errors && code != NULL, backend, code,
                                 &diags, NULL, 0, 0);
  free(code);
  diag_list_free(&diags);
  compiler_result_free(&parsed);
  return json;
}
