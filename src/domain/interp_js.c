#include "domain/interp_internal.h"
#include <stdlib.h>
#include <string.h>

char *interp_to_js_template_body(const char *str) {
  if (!str) return strdup("");
  if (!interp_has(str)) {
    /* escape for template literal even without interp — caller may still wrap */
    size_t cap = strlen(str) * 2 + 1;
    char *out = malloc(cap);
    size_t o = 0;
    for (const char *p = str; *p; p++) {
      if (o + 2 >= cap) {
        cap *= 2;
        out = realloc(out, cap);
      }
      if (*p == '`' || *p == '\\' || (*p == '$' && p[1] == '{')) {
        out[o++] = '\\';
      }
      out[o++] = *p;
    }
    out[o] = '\0';
    return out;
  }

  size_t cap = strlen(str) * 2 + 64;
  char *out = malloc(cap);
  size_t o = 0;

  const char *p = str;
  while (*p) {
    const char *hash = NULL;
    int escaped = 0;
    for (const char *s = p; *s; s++) {
      if (s[0] == '#' && s[1] == '{') {
        if (s > str && s[-1] == '\\') {
          hash = s;
          escaped = 1;
          break;
        }
        hash = s;
        escaped = 0;
        break;
      }
    }
    if (!hash) {
      for (const char *s = p; *s; s++) {
        if (o + 2 >= cap) {
          cap *= 2;
          out = realloc(out, cap);
        }
        if (*s == '`' || *s == '\\' || (*s == '$' && s[1] == '{')) out[o++] = '\\';
        out[o++] = *s;
      }
      break;
    }

    if (escaped) {
      /* Literal \#{…} → emit "#{…}" (drop backslash) into template body */
      const char *close = interp_find_end(hash);
      for (const char *s = p; s < hash - 1; s++) {
        if (o + 2 >= cap) {
          cap *= 2;
          out = realloc(out, cap);
        }
        if (*s == '`' || *s == '\\' || (*s == '$' && s[1] == '{')) out[o++] = '\\';
        out[o++] = *s;
      }
      if (!close) {
        for (const char *s = hash; *s; s++) {
          if (o + 2 >= cap) {
            cap *= 2;
            out = realloc(out, cap);
          }
          if (*s == '`' || *s == '\\') out[o++] = '\\';
          out[o++] = *s;
        }
        break;
      }
      for (const char *s = hash; s <= close; s++) {
        if (o + 2 >= cap) {
          cap *= 2;
          out = realloc(out, cap);
        }
        if (*s == '`' || *s == '\\' || (*s == '$' && s[1] == '{')) out[o++] = '\\';
        out[o++] = *s;
      }
      p = close + 1;
      continue;
    }

    for (const char *s = p; s < hash; s++) {
      if (o + 2 >= cap) {
        cap *= 2;
        out = realloc(out, cap);
      }
      if (*s == '`' || *s == '\\' || (*s == '$' && s[1] == '{')) out[o++] = '\\';
      out[o++] = *s;
    }

    const char *close = interp_find_end(hash);
    if (!close) {
      for (const char *s = hash; *s; s++) {
        if (o + 2 >= cap) {
          cap *= 2;
          out = realloc(out, cap);
        }
        if (*s == '`' || *s == '\\') out[o++] = '\\';
        out[o++] = *s;
      }
      break;
    }

    char *expr = interp_trim_dup(hash + 2, (size_t)(close - (hash + 2)));
    size_t elen = expr ? strlen(expr) : 0;
    if (o + elen + 4 >= cap) {
      cap = (o + elen + 4) * 2;
      out = realloc(out, cap);
    }
    out[o++] = '$';
    out[o++] = '{';
    if (expr && elen) {
      memcpy(out + o, expr, elen);
      o += elen;
    }
    out[o++] = '}';
    free(expr);
    p = close + 1;
  }
  out[o] = '\0';
  return out;
}
