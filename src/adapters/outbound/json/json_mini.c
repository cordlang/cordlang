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
