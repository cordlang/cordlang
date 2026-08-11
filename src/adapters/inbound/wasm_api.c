#include "application/compile_service.h"
#include "application/check_service.h"
#include "application/ports/backend_port.h"
#include "application/ports/compiler_port.h"
#include "application/ports/fs_port.h"
#include "domain/diag.h"
#include "domain/ir.h"
#include "domain/version.h"

#include <ctype.h>
#include <stdint.h>
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
 *   _cordlang_compile_project — same result for an in-memory file map
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

CORD_WASM_EXPORT const char *cordlang_version(void) { return CORDLANG_VERSION; }

CORD_WASM_EXPORT void cordlang_free(char *p) { free(p); }

typedef struct {
  FsMemoryFile *items;
  size_t len;
  size_t cap;
} WasmProjectFiles;

#define WASM_PROJECT_MAX_FILES 128

static void wasm_skip_ws(const char **p) {
  while (*p && **p && isspace((unsigned char)**p)) (*p)++;
}

static int wasm_buf_push(char **buf, size_t *len, size_t *cap,
                         unsigned char byte) {
  if (*len > SIZE_MAX - 2) return 0;
  if (*len + 2 > *cap) {
    size_t next = *cap ? *cap * 2 : 64;
    while (*len + 2 > next) {
      if (next > SIZE_MAX / 2) return 0;
      next *= 2;
    }
    char *grown = realloc(*buf, next);
    if (!grown) return 0;
    *buf = grown;
    *cap = next;
  }
  (*buf)[(*len)++] = (char)byte;
  (*buf)[*len] = '\0';
  return 1;
}

static int wasm_hex_value(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

static int wasm_parse_hex4(const char **p, uint32_t *out) {
  uint32_t value = 0;
  for (int i = 0; i < 4; i++) {
    int digit = wasm_hex_value((*p)[i]);
    if (digit < 0) return 0;
    value = (value << 4) | (uint32_t)digit;
  }
  *p += 4;
  *out = value;
  return 1;
}

static int wasm_push_utf8(char **buf, size_t *len, size_t *cap,
                          uint32_t codepoint) {
  if (codepoint == 0 || codepoint > 0x10ffff ||
      (codepoint >= 0xd800 && codepoint <= 0xdfff))
    return 0;
  if (codepoint <= 0x7f)
    return wasm_buf_push(buf, len, cap, (unsigned char)codepoint);
  if (codepoint <= 0x7ff)
    return wasm_buf_push(buf, len, cap, 0xc0 | (codepoint >> 6)) &&
           wasm_buf_push(buf, len, cap, 0x80 | (codepoint & 0x3f));
  if (codepoint <= 0xffff)
    return wasm_buf_push(buf, len, cap, 0xe0 | (codepoint >> 12)) &&
           wasm_buf_push(buf, len, cap, 0x80 | ((codepoint >> 6) & 0x3f)) &&
           wasm_buf_push(buf, len, cap, 0x80 | (codepoint & 0x3f));
  return wasm_buf_push(buf, len, cap, 0xf0 | (codepoint >> 18)) &&
         wasm_buf_push(buf, len, cap, 0x80 | ((codepoint >> 12) & 0x3f)) &&
         wasm_buf_push(buf, len, cap, 0x80 | ((codepoint >> 6) & 0x3f)) &&
         wasm_buf_push(buf, len, cap, 0x80 | (codepoint & 0x3f));
}

/* Parse one strict JSON string into owned UTF-8 bytes. */
static int wasm_parse_json_string(const char **p, char **out,
                                  size_t *out_len) {
  if (!p || !*p || **p != '"') return 0;
  (*p)++;
  char *buf = NULL;
  size_t len = 0, cap = 0;
  while (**p && **p != '"') {
    unsigned char c = (unsigned char)*(*p)++;
    if (c < 0x20) goto fail;
    if (c != '\\') {
      if (!wasm_buf_push(&buf, &len, &cap, c)) goto fail;
      continue;
    }
    char esc = *(*p)++;
    if (!esc) goto fail;
    switch (esc) {
      case '"': if (!wasm_buf_push(&buf, &len, &cap, '"')) goto fail; break;
      case '\\': if (!wasm_buf_push(&buf, &len, &cap, '\\')) goto fail; break;
      case '/': if (!wasm_buf_push(&buf, &len, &cap, '/')) goto fail; break;
      case 'b': if (!wasm_buf_push(&buf, &len, &cap, '\b')) goto fail; break;
      case 'f': if (!wasm_buf_push(&buf, &len, &cap, '\f')) goto fail; break;
      case 'n': if (!wasm_buf_push(&buf, &len, &cap, '\n')) goto fail; break;
      case 'r': if (!wasm_buf_push(&buf, &len, &cap, '\r')) goto fail; break;
      case 't': if (!wasm_buf_push(&buf, &len, &cap, '\t')) goto fail; break;
      case 'u': {
        uint32_t codepoint = 0;
        if (!wasm_parse_hex4(p, &codepoint)) goto fail;
        if (codepoint >= 0xd800 && codepoint <= 0xdbff) {
          if ((*p)[0] != '\\' || (*p)[1] != 'u') goto fail;
          *p += 2;
          uint32_t low = 0;
          if (!wasm_parse_hex4(p, &low) || low < 0xdc00 || low > 0xdfff)
            goto fail;
          codepoint = 0x10000 + ((codepoint - 0xd800) << 10) +
                      (low - 0xdc00);
        }
        if (!wasm_push_utf8(&buf, &len, &cap, codepoint)) goto fail;
        break;
      }
      default: goto fail;
    }
  }
  if (**p != '"') goto fail;
  (*p)++;
  if (!buf) {
    buf = strdup("");
    if (!buf) return 0;
  }
  *out = buf;
  if (out_len) *out_len = len;
  return 1;

fail:
  free(buf);
  return 0;
}

static void wasm_project_files_free(WasmProjectFiles *project) {
  if (!project) return;
  for (size_t i = 0; i < project->len; i++) {
    free((char *)project->items[i].path);
    free((char *)project->items[i].data);
  }
  free(project->items);
  project->items = NULL;
  project->len = 0;
  project->cap = 0;
}

static int wasm_project_add_file(WasmProjectFiles *project, char *path,
                                 char *data, size_t len) {
  if (!project || !path || !data || path[0] != '/' ||
      project->len >= WASM_PROJECT_MAX_FILES)
    return 0;
  if (project->len == project->cap) {
    size_t next = project->cap ? project->cap * 2 : 8;
    FsMemoryFile *grown = realloc(project->items, next * sizeof(*grown));
    if (!grown) return 0;
    project->items = grown;
    project->cap = next;
  }
  project->items[project->len].path = path;
  project->items[project->len].data = data;
  project->items[project->len].len = len;
  project->len++;
  return 1;
}

/* Manifest is a JSON object: { "/project/src/app.cord": "...", ... }. */
static int wasm_parse_project_manifest(const char *manifest,
                                       WasmProjectFiles *project,
                                       const char **error) {
  if (!manifest || !project) {
    if (error) *error = "missing project manifest";
    return 0;
  }
  const char *p = manifest;
  wasm_skip_ws(&p);
  if (*p++ != '{') {
    if (error) *error = "project manifest must be a JSON object";
    return 0;
  }
  wasm_skip_ws(&p);
  while (*p && *p != '}') {
    char *path = NULL;
    char *data = NULL;
    size_t data_len = 0;
    if (!wasm_parse_json_string(&p, &path, NULL) || path[0] != '/') {
      free(path);
      if (error) *error = "project file paths must be absolute JSON strings";
      return 0;
    }
    wasm_skip_ws(&p);
    if (*p++ != ':') {
      free(path);
      if (error) *error = "project manifest is missing ':' after a path";
      return 0;
    }
    wasm_skip_ws(&p);
    if (!wasm_parse_json_string(&p, &data, &data_len) ||
        !wasm_project_add_file(project, path, data, data_len)) {
      free(path);
      free(data);
      if (error) *error = "invalid project file or too many files";
      return 0;
    }
    wasm_skip_ws(&p);
    if (*p == ',') {
      p++;
      wasm_skip_ws(&p);
      if (*p == '}') {
        if (error) *error = "project manifest cannot end with ','";
        return 0;
      }
      continue;
    }
    if (*p != '}') {
      if (error) *error = "project manifest expects ',' or '}'";
      return 0;
    }
  }
  if (*p != '}') {
    if (error) *error = "unterminated project manifest";
    return 0;
  }
  p++;
  wasm_skip_ws(&p);
  if (*p != '\0' || project->len == 0) {
    if (error) *error = project->len == 0 ? "project manifest has no files"
                                           : "trailing project manifest data";
    return 0;
  }
  return 1;
}

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

/*
 * Compile a virtual multi-file project. entry_path and every manifest key must
 * be absolute virtual paths. The map is mounted only for this call; project
 * resolution cannot read undeclared host files.
 */
CORD_WASM_EXPORT char *cordlang_compile_project(const char *entry_path,
                                                const char *files_json,
                                                const char *backend) {
  if (!backend || !*backend) backend = "react";
  if (!entry_path || entry_path[0] != '/')
    return build_result_json(0, backend, NULL, NULL,
                             "entry path must be an absolute virtual path", 0,
                             0);

  WasmProjectFiles project = {0};
  const char *manifest_error = NULL;
  if (!wasm_parse_project_manifest(files_json, &project, &manifest_error)) {
    wasm_project_files_free(&project);
    return build_result_json(0, backend, NULL, NULL,
                             manifest_error ? manifest_error
                                            : "invalid project manifest",
                             0, 0);
  }
  if (fs_memory_mount(project.items, project.len, "/") != 0) {
    wasm_project_files_free(&project);
    return build_result_json(0, backend, NULL, NULL,
                             "cannot mount virtual project filesystem", 0,
                             0);
  }
  wasm_project_files_free(&project);

  backend_register_all();
  DiagList diags;
  diag_list_init(&diags);
  int has_errors = check_service_run(entry_path, &diags) != 0;
  char *code = NULL;
  if (!has_errors) {
    code = compile_service_file_with_passes(entry_path, backend, 0, NULL, NULL,
                                            0);
    if (!code) {
      diag_emit(&diags, DIAG_ERROR, entry_path, 1, 1,
                "codegen failed for backend '%s'", backend);
      has_errors = 1;
    }
  }

  char *json = build_result_json(!has_errors && code != NULL, backend, code,
                                 &diags, NULL, 0, 0);
  free(code);
  diag_list_free(&diags);
  fs_memory_unmount();
  return json;
}
