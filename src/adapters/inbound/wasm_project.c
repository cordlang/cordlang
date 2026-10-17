#include "adapters/inbound/wasm_internal.h"
#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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
void wasm_project_files_free(WasmProjectFiles *project) {
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
int wasm_parse_project_manifest(const char *manifest,
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
