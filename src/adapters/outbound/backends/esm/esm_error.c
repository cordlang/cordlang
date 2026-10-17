#include "adapters/outbound/backends/esm/esm_backend.h"
#include "adapters/outbound/html_escape.h"
#include "domain/diag.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
char *esm_error_module(const char *message) {
  DiagList diags;
  diag_list_init(&diags);
  diag_emit(&diags, DIAG_ERROR, "cordlang", 0, 0, "%s",
            message ? message : "error desconocido");
  char *out = esm_error_module_from_diags(&diags, NULL, 0);
  diag_list_free(&diags);
  return out;
}

static void frame_json_append(char **buf, size_t *len, size_t *cap,
                              const char *s) {
  if (!s) return;
  size_t n = strlen(s);
  if (*len + n + 1 > *cap) {
    size_t ncap = *cap ? *cap * 2 : 512;
    while (ncap < *len + n + 1) ncap *= 2;
    char *nb = realloc(*buf, ncap);
    if (!nb) return;
    *buf = nb;
    *cap = ncap;
  }
  memcpy(*buf + *len, s, n);
  *len += n;
  (*buf)[*len] = '\0';
}

static void frame_json_escape(char **buf, size_t *len, size_t *cap,
                              const char *s) {
  if (!s) return;
  for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
    char tmp[8];
    switch (*p) {
      case '"':
        frame_json_append(buf, len, cap, "\\\"");
        break;
      case '\\':
        frame_json_append(buf, len, cap, "\\\\");
        break;
      case '\n':
        frame_json_append(buf, len, cap, "\\n");
        break;
      case '\r':
        frame_json_append(buf, len, cap, "\\r");
        break;
      case '\t':
        frame_json_append(buf, len, cap, "\\t");
        break;
      default:
        if (*p < 0x20) {
          snprintf(tmp, sizeof(tmp), "\\u%04x", *p);
          frame_json_append(buf, len, cap, tmp);
        } else {
          tmp[0] = (char)*p;
          tmp[1] = '\0';
          frame_json_append(buf, len, cap, tmp);
        }
        break;
    }
  }
}

/* Build { "file", "startLine", "lines": [...] } around the first diag with line>0. */
static char *build_frame_json(const DiagList *diags, const char *source,
                              size_t source_len) {
  if (!diags || !diags->len || !source) return NULL;
  const Diagnostic *hit = NULL;
  for (size_t i = 0; i < diags->len; i++) {
    if (diags->items[i].line > 0) {
      hit = &diags->items[i];
      break;
    }
  }
  if (!hit) hit = &diags->items[0];
  if (hit->line <= 0) return NULL;

  int err_line = hit->line;
  int start = err_line - 3;
  if (start < 1) start = 1;
  int end = err_line + 3;

  /* Collect lines 1..end */
  const char *p = source;
  size_t remain = source_len;
  int lineno = 1;
  char *out = NULL;
  size_t len = 0, cap = 0;
  frame_json_append(&out, &len, &cap, "{\"file\":\"");
  frame_json_escape(&out, &len, &cap, hit->file ? hit->file : "<input>");
  {
    char hdr[64];
    snprintf(hdr, sizeof(hdr), "\",\"startLine\":%d,\"lines\":[", start);
    frame_json_append(&out, &len, &cap, hdr);
  }
  int wrote = 0;
  while (remain > 0 && lineno <= end) {
    const char *nl = memchr(p, '\n', remain);
    size_t line_len = nl ? (size_t)(nl - p) : remain;
    if (lineno >= start) {
      if (wrote) frame_json_append(&out, &len, &cap, ",");
      frame_json_append(&out, &len, &cap, "\"");
      /* strip trailing \r */
      size_t use = line_len;
      if (use > 0 && p[use - 1] == '\r') use--;
      char *tmp = malloc(use + 1);
      if (tmp) {
        memcpy(tmp, p, use);
        tmp[use] = '\0';
        frame_json_escape(&out, &len, &cap, tmp);
        free(tmp);
      }
      frame_json_append(&out, &len, &cap, "\"");
      wrote = 1;
    }
    if (!nl) break;
    size_t step = line_len + 1;
    p += step;
    remain -= step;
    lineno++;
  }
  frame_json_append(&out, &len, &cap, "]}");
  return out;
}

char *esm_error_module_from_diags(const DiagList *diags, const char *source,
                                  size_t source_len) {
  char *diag_json = diag_format_json(diags);
  char *frame_json = build_frame_json(diags, source, source_len);

  size_t payload_cap = (diag_json ? strlen(diag_json) : 2) +
                       (frame_json ? strlen(frame_json) : 0) + 128;
  char *payload = malloc(payload_cap);
  if (!payload) {
    free(diag_json);
    free(frame_json);
    return NULL;
  }
  if (frame_json) {
    snprintf(payload, payload_cap,
             "{\"title\":\"Failed to compile\",\"diagnostics\":%s,\"frame\":%s}",
             diag_json ? diag_json : "[]", frame_json);
  } else {
    snprintf(payload, payload_cap,
             "{\"title\":\"Failed to compile\",\"diagnostics\":%s}",
             diag_json ? diag_json : "[]");
  }
  free(diag_json);
  free(frame_json);

  char *esc = js_escape_dq_dup(payload);
  free(payload);
  if (!esc) return NULL;

  size_t need = strlen(esc) + 256;
  char *out = malloc(need);
  if (!out) {
    free(esc);
    return NULL;
  }
  snprintf(out, need,
           "import { showCompileError } from '/@cord/runtime.js';\n"
           "showCompileError(JSON.parse(\"%s\"));\n"
           "export default null;\n",
           esc);
  free(esc);
  return out;
}
