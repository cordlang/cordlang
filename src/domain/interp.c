#include "domain/interp.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int interp_has(const char *s) {
  if (!s) return 0;
  for (const char *p = s; *p; p++) {
    if (p[0] == '#' && p[1] == '{') {
      /* \#\{ is a literal, not interpolation */
      if (p > s && p[-1] == '\\') continue;
      return 1;
    }
  }
  return 0;
}

/* Drop backslash before \#{…} for display as literal "#{…}". */
char *interp_plain_text(const char *s) {
  if (!s) return strdup("");
  size_t n = strlen(s);
  char *out = malloc(n + 1);
  if (!out) return NULL;
  size_t j = 0;
  for (size_t i = 0; i < n; i++) {
    if (s[i] == '\\' && i + 2 < n && s[i + 1] == '#' && s[i + 2] == '{') {
      /* skip '\\'; keep "#{…}" */
      continue;
    }
    out[j++] = s[i];
  }
  out[j] = '\0';
  return out;
}

/* Trim leading/trailing whitespace in-place copy; returns malloc'd string */
static char *trim_dup(const char *start, size_t len) {
  while (len > 0 && isspace((unsigned char)*start)) {
    start++;
    len--;
  }
  while (len > 0 && isspace((unsigned char)start[len - 1])) len--;
  char *s = malloc(len + 1);
  if (!s) return NULL;
  if (len) memcpy(s, start, len);
  s[len] = '\0';
  return s;
}

static char *substr_dup(const char *start, size_t len) {
  char *s = malloc(len + 1);
  if (!s) return NULL;
  if (len) memcpy(s, start, len);
  s[len] = '\0';
  return s;
}

/* Find end of #{...} starting at p pointing to '#'. Returns pointer to '}' or NULL. */
static const char *find_interp_end(const char *p) {
  if (!p || p[0] != '#' || p[1] != '{') return NULL;
  const char *q = p + 2;
  int depth = 1;
  while (*q) {
    if (*q == '{')
      depth++;
    else if (*q == '}') {
      depth--;
      if (depth == 0) return q;
    }
    q++;
  }
  return NULL; /* unclosed */
}

void interp_add_to_node(Node *parent, const char *str, int line, int col) {
  if (!parent) return;
  if (!str) {
    node_add_child(parent, node_create(NODE_TEXT, "", line, col));
    return;
  }
  if (!interp_has(str)) {
    node_add_child(parent, node_create(NODE_TEXT, str, line, col));
    return;
  }

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
      node_add_child(parent, node_create(NODE_TEXT, p, line, col));
      break;
    }

    if (escaped) {
      /* Keep \#{…} in TEXT (with backslash) so emitters don't re-interp;
       * emit paths call interp_plain_text to drop the '\'. */
      const char *close = find_interp_end(hash);
      size_t before = (size_t)((hash - 1) - p); /* up to but not including \ */
      if (before > 0) {
        char *lit = substr_dup(p, before);
        node_add_child(parent, node_create(NODE_TEXT, lit, line, col));
        free(lit);
      }
      if (!close) {
        node_add_child(parent, node_create(NODE_TEXT, hash - 1, line, col));
        break;
      }
      char *lit = substr_dup(hash - 1, (size_t)(close - (hash - 1) + 1));
      node_add_child(parent, node_create(NODE_TEXT, lit, line, col));
      free(lit);
      p = close + 1;
      continue;
    }

    if (hash > p) {
      char *lit = substr_dup(p, (size_t)(hash - p));
      node_add_child(parent, node_create(NODE_TEXT, lit, line, col));
      free(lit);
    }

    const char *close = find_interp_end(hash);
    if (!close) {
      node_add_child(parent, node_create(NODE_TEXT, hash, line, col));
      break;
    }

    char *expr = trim_dup(hash + 2, (size_t)(close - (hash + 2)));
    node_add_child(parent,
                   node_create(NODE_INTERPOLATION, expr ? expr : "", line, col));
    free(expr);

    p = close + 1;
  }
}

Node *interp_make_node(const char *str, int line, int col) {
  if (!str || !interp_has(str)) {
    return node_create(NODE_TEXT, str ? str : "", line, col);
  }
  Node *box = node_create(NODE_INTERPOLATION, NULL, line, col);
  interp_add_to_node(box, str, line, col);
  return box;
}

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
      const char *close = find_interp_end(hash);
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

    const char *close = find_interp_end(hash);
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

    char *expr = trim_dup(hash + 2, (size_t)(close - (hash + 2)));
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
      const char *close = find_interp_end(hash);
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

    const char *close = find_interp_end(hash);
    if (!close) {
      for (const char *s = hash; *s; s++) APP_ESC(*s);
      break;
    }
    char *expr = trim_dup(hash + 2, (size_t)(close - (hash + 2)));
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
