#include "application/symbols_internal.h"

static int ident_char(int c) {
  return isalnum((unsigned char)c) || c == '_';
}

int symbols_valid_component_name(const char *name) {
  static const char *reserved[] = {
      "def",     "state",  "props",    "computed", "route",  "layout",
      "theme",   "use",    "import",   "as",       "from",   "effect",
      "ref",     "ctx",    "provide",  "params",   "fetch",  "lazy",
      "if",      "else",   "for",      "in",       "key",    "slot",
      "foreign", "head",   "loading",  "empty",    "await",  "snippet",
      "render",  "portal", "suspense", "store",    NULL};
  if (!name || !*name) return 0;
  if (!(isalpha((unsigned char)name[0]) || name[0] == '_')) return 0;
  for (const char *p = name + 1; *p; p++) {
    if (!ident_char((unsigned char)*p)) return 0;
  }
  for (int i = 0; reserved[i]; i++) {
    if (strcmp(name, reserved[i]) == 0) return 0;
  }
  if (cord_is_builtin_tag(name)) return 0;
  if (cord_is_jsx_hook_name(name)) return 0;
  return 1;
}

int path_same(const char *a, const char *b) {
  if (!a || !b) return 0;
  char *na = fs_norm_path(a);
  char *nb = fs_norm_path(b);
  int eq = 0;
  if (na && nb) {
#ifdef _WIN32
    eq = STRICMP(na, nb) == 0;
#else
    eq = strcmp(na, nb) == 0;
#endif
  }
  free(na);
  free(nb);
  return eq;
}

void str_list_add(PathList *pl, const char *s) {
  if (!pl || !s || !*s) return;
  for (size_t i = 0; i < pl->n; i++) {
    if (strcmp(pl->paths[i], s) == 0) return;
  }
  if (pl->n + 1 >= pl->cap) {
    size_t ncap = pl->cap ? pl->cap * 2 : 16;
    char **np = realloc(pl->paths, ncap * sizeof(char *));
    if (!np) return;
    pl->paths = np;
    pl->cap = ncap;
  }
  pl->paths[pl->n++] = strdup(s);
}

void path_list_add(PathList *pl, const char *path) {
  if (!pl || !path || !*path) return;
  for (size_t i = 0; i < pl->n; i++) {
    if (path_same(pl->paths[i], path)) return;
  }
  if (pl->n + 1 >= pl->cap) {
    size_t ncap = pl->cap ? pl->cap * 2 : 16;
    char **np = realloc(pl->paths, ncap * sizeof(char *));
    if (!np) return;
    pl->paths = np;
    pl->cap = ncap;
  }
  pl->paths[pl->n++] = strdup(path);
}

void path_list_free(PathList *pl) {
  if (!pl) return;
  for (size_t i = 0; i < pl->n; i++) free(pl->paths[i]);
  free(pl->paths);
  memset(pl, 0, sizeof(*pl));
}

void walk_add_path(const char *abs, const char *rel, void *ud) {
  (void)rel;
  path_list_add((PathList *)ud, abs);
}

static const char *line_at(const char *text, int line_1, size_t *out_len) {
  if (!text || line_1 < 1) return NULL;
  int ln = 1;
  const char *p = text;
  while (*p && ln < line_1) {
    if (*p == '\n') ln++;
    p++;
  }
  if (ln != line_1) return NULL;
  const char *start = p;
  while (*p && *p != '\n' && *p != '\r') p++;
  if (out_len) *out_len = (size_t)(p - start);
  return start;
}

/* 0-based columns of word-boundary `name` on a line, skipping strings. */
static int line_name_cols(const char *line, size_t llen, const char *name,
                          int *cols, int max_cols) {
  if (!line || !name || !cols || max_cols <= 0) return 0;
  size_t nlen = strlen(name);
  if (nlen == 0) return 0;
  int n = 0;
  int in_str = 0;
  for (size_t i = 0; i < llen;) {
    char c = line[i];
    if (!in_str && c == '"') {
      in_str = 1;
      i++;
      continue;
    }
    if (in_str) {
      if (c == '\\' && i + 1 < llen) {
        i += 2;
        continue;
      }
      if (c == '"') in_str = 0;
      i++;
      continue;
    }
    if (c == '#' && (i + 1 >= llen || line[i + 1] != '{')) break;
    if (c == '/' && i + 1 < llen && line[i + 1] == '/') break;
    if (i + nlen <= llen && strncmp(line + i, name, nlen) == 0) {
      int before = (i == 0) || !ident_char((unsigned char)line[i - 1]);
      int after =
          (i + nlen >= llen) || !ident_char((unsigned char)line[i + nlen]);
      if (before && after && n < max_cols) cols[n++] = (int)i;
      i += nlen;
      continue;
    }
    i++;
  }
  return n;
}

/* Pick a 1-based start column for `name` on `line_1`. hint_col_1 is the AST
 * token column (lexer stores the exclusive end). prefer_last for path segs. */
int pick_name_col(const char *text, int line_1, int hint_col_1,
                         const char *name, int prefer_last) {
  size_t llen = 0;
  const char *line = line_at(text, line_1, &llen);
  if (!line || !name) return 0;
  int cols[16];
  int n = line_name_cols(line, llen, name, cols, 16);
  if (n <= 0) return 0;
  if (n == 1) return cols[0] + 1;
  size_t nlen = strlen(name);
  int hint0 = hint_col_1 > 0 ? hint_col_1 - 1 : 0;
  int best = prefer_last ? cols[n - 1] : cols[0];
  int best_d = 100000;
  for (int i = 0; i < n; i++) {
    int end0 = cols[i] + (int)nlen;
    int d = abs(end0 - hint0);
    int d2 = abs(cols[i] - hint0);
    if (d2 < d) d = d2;
    if (d < best_d) {
      best_d = d;
      best = cols[i];
    }
  }
  return best + 1;
}

