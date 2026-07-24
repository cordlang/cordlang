#include "adapters/outbound/json/json_mini.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *skip_ws(const char *p) {
  while (p && *p && isspace((unsigned char)*p)) p++;
  return p;
}

int json_object_copy_string(const char *json, const char *key, char *out,
                            size_t out_sz) {
  if (!json || !key || !out || out_sz == 0) return 0;
  out[0] = '\0';

  char pat[96];
  if (snprintf(pat, sizeof(pat), "\"%s\"", key) >= (int)sizeof(pat)) return 0;

  const char *p = json;
  while ((p = strstr(p, pat)) != NULL) {
    const char *after = skip_ws(p + strlen(pat));
    if (*after != ':') {
      p += strlen(pat);
      continue;
    }
    after = skip_ws(after + 1);
    if (*after != '"') return 0;
    after++;
    size_t i = 0;
    while (*after && *after != '"' && i + 1 < out_sz) {
      if (*after == '\\' && after[1]) {
        after++;
        out[i++] = *after++;
        continue;
      }
      out[i++] = *after++;
    }
    if (*after != '"') return 0; /* truncated / unclosed */
    out[i] = '\0';
    return 1;
  }
  return 0;
}

char *json_object_get_string(const char *json, const char *key) {
  char buf[1024];
  if (!json_object_copy_string(json, key, buf, sizeof(buf))) return NULL;
  return strdup(buf);
}

static int parse_string_array_at(const char *arr, char *names, size_t elem_sz,
                                 int max_names) {
  if (!arr || !names || elem_sz == 0 || max_names <= 0) return -1;
  const char *p = skip_ws(arr);
  if (*p != '[') return -1;
  p++;
  int n = 0;
  while (*p && n < max_names) {
    p = skip_ws(p);
    if (*p == ']') break;
    if (*p == ',') {
      p++;
      continue;
    }
    if (*p != '"') return n; /* stop on unexpected */
    p++;
    size_t i = 0;
    char *slot = names + (size_t)n * elem_sz;
    while (*p && *p != '"' && i + 1 < elem_sz) {
      if (*p == '\\' && p[1]) {
        p++;
        slot[i++] = *p++;
        continue;
      }
      slot[i++] = *p++;
    }
    if (*p != '"') return -1;
    p++;
    slot[i] = '\0';
    n++;
  }
  return n;
}

int json_object_copy_string_array(const char *json, const char *key, char *names,
                                  size_t elem_sz, int max_names) {
  if (!json || !key || !names) return -1;
  char pat[96];
  if (snprintf(pat, sizeof(pat), "\"%s\"", key) >= (int)sizeof(pat)) return -1;
  const char *p = json;
  while ((p = strstr(p, pat)) != NULL) {
    const char *after = skip_ws(p + strlen(pat));
    if (*after != ':') {
      p += strlen(pat);
      continue;
    }
    after = skip_ws(after + 1);
    return parse_string_array_at(after, names, elem_sz, max_names);
  }
  return 0;
}

int json_object_nested_string_array(const char *json, const char *parent,
                                    const char *key, char *names, size_t elem_sz,
                                    int max_names) {
  if (!json || !parent || !key || !names) return -1;
  char pat[96];
  if (snprintf(pat, sizeof(pat), "\"%s\"", parent) >= (int)sizeof(pat))
    return -1;
  const char *p = strstr(json, pat);
  if (!p) return 0;
  p = skip_ws(p + strlen(pat));
  if (*p != ':') return 0;
  p = skip_ws(p + 1);
  if (*p != '{') return 0;
  /* Search key only within this object (best-effort: until matching }). */
  const char *start = p;
  int depth = 0;
  const char *end = start;
  do {
    if (*end == '{') depth++;
    else if (*end == '}') depth--;
    end++;
  } while (*end && depth > 0);
  size_t region_len = (size_t)(end - start);
  char *region = malloc(region_len + 1);
  if (!region) return -1;
  memcpy(region, start, region_len);
  region[region_len] = '\0';
  int n = json_object_copy_string_array(region, key, names, elem_sz, max_names);
  free(region);
  return n;
}
