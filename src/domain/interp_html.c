#include "domain/interp_internal.h"
#include <stdlib.h>
#include <string.h>

char *interp_to_html_fragment(const char *str) {
  if (!str) return strdup("");
  if (!interp_has(str)) {
    /* escape HTML */
    size_t cap = strlen(str) * 6 + 1;
    char *out = malloc(cap);
    size_t o = 0;
    for (const char *p = str; *p; p++) {
      const char *rep = NULL;
      switch (*p) {
        case '&': rep = "&amp;"; break;
        case '<': rep = "&lt;"; break;
        case '>': rep = "&gt;"; break;
        case '"': rep = "&quot;"; break;
        default: break;
      }
      if (rep) {
        size_t rl = strlen(rep);
        if (o + rl >= cap) {
          cap *= 2;
          out = realloc(out, cap);
        }
        memcpy(out + o, rep, rl);
        o += rl;
      } else {
        if (o + 1 >= cap) {
          cap *= 2;
          out = realloc(out, cap);
        }
        out[o++] = *p;
      }
    }
    out[o] = '\0';
    return out;
  }

  size_t cap = strlen(str) * 8 + 128;
  char *out = malloc(cap);
  size_t o = 0;

#define APP(s)                                                                 \
  do {                                                                         \
    const char *_s = (s);                                                      \
    size_t _n = strlen(_s);                                                    \
    if (o + _n + 1 >= cap) {                                                   \
      cap = (o + _n + 1) * 2;                                                  \
      out = realloc(out, cap);                                                 \
    }                                                                          \
    memcpy(out + o, _s, _n);                                                   \
    o += _n;                                                                   \
  } while (0)

#define APP_ESC(ch)                                                            \
  do {                                                                         \
    char _c = (ch);                                                            \
    if (_c == '&')                                                             \
      APP("&amp;");                                                            \
    else if (_c == '<')                                                        \
      APP("&lt;");                                                             \
    else if (_c == '>')                                                        \
      APP("&gt;");                                                             \
    else if (_c == '"')                                                        \
      APP("&quot;");                                                           \
    else {                                                                     \
      if (o + 1 >= cap) {                                                      \
        cap *= 2;                                                              \
        out = realloc(out, cap);                                               \
      }                                                                        \
      out[o++] = _c;                                                           \
    }                                                                          \
  } while (0)

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
      for (const char *s = p; *s; s++) APP_ESC(*s);
      break;
    }
    if (escaped) {
      const char *close = interp_find_end(hash);
      for (const char *s = p; s < hash - 1; s++) APP_ESC(*s);
      if (!close) {
        for (const char *s = hash; *s; s++) APP_ESC(*s);
        break;
      }
      for (const char *s = hash; s <= close; s++) APP_ESC(*s);
      p = close + 1;
      continue;
    }
    for (const char *s = p; s < hash; s++) APP_ESC(*s);

    const char *close = interp_find_end(hash);
    if (!close) {
      for (const char *s = hash; *s; s++) APP_ESC(*s);
      break;
    }
    char *expr = interp_trim_dup(hash + 2, (size_t)(close - (hash + 2)));
    APP("<span class=\"cl-interp\" title=\"");
    if (expr) {
      for (const char *e = expr; *e; e++) APP_ESC(*e);
    }
    APP("\">${");
    if (expr) {
      for (const char *e = expr; *e; e++) APP_ESC(*e);
    }
    APP("}</span>");
    free(expr);
    p = close + 1;
  }
  out[o] = '\0';
#undef APP
#undef APP_ESC
  return out;
}
