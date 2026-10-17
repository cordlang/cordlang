#include "domain/interp_internal.h"
#include <ctype.h>
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
char *interp_trim_dup(const char *start, size_t len) {
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
char *interp_substr_dup(const char *start, size_t len) {
  char *s = malloc(len + 1);
  if (!s) return NULL;
  if (len) memcpy(s, start, len);
  s[len] = '\0';
  return s;
}
const char *interp_find_end(const char *p) {
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
      const char *close = interp_find_end(hash);
      size_t before = (size_t)((hash - 1) - p); /* up to but not including \ */
      if (before > 0) {
        char *lit = interp_substr_dup(p, before);
        node_add_child(parent, node_create(NODE_TEXT, lit, line, col));
        free(lit);
      }
      if (!close) {
        node_add_child(parent, node_create(NODE_TEXT, hash - 1, line, col));
        break;
      }
      char *lit = interp_substr_dup(hash - 1, (size_t)(close - (hash - 1) + 1));
      node_add_child(parent, node_create(NODE_TEXT, lit, line, col));
      free(lit);
      p = close + 1;
      continue;
    }

    if (hash > p) {
      char *lit = interp_substr_dup(p, (size_t)(hash - p));
      node_add_child(parent, node_create(NODE_TEXT, lit, line, col));
      free(lit);
    }

    const char *close = interp_find_end(hash);
    if (!close) {
      node_add_child(parent, node_create(NODE_TEXT, hash, line, col));
      break;
    }

    char *expr = interp_trim_dup(hash + 2, (size_t)(close - (hash + 2)));
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
