#include "adapters/outbound/compiler/compiler_internal.h"
#include "adapters/outbound/lexer/lexer.h"
#include "adapters/outbound/parser/parser.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int visited_has(VisitedSet *v, const char *path) {
  for (int i = 0; i < v->count; i++) {
    if (strcmp(v->paths[i], path) == 0) return 1;
  }
  return 0;
}

void visited_add(VisitedSet *v, const char *path) {
  if (v->count >= MAX_VISITED) return;
  if (visited_has(v, path)) return;
  v->paths[v->count++] = strdup(path);
}

void visited_free(VisitedSet *v) {
  for (int i = 0; i < v->count; i++) free(v->paths[i]);
  v->count = 0;
}

char *module_export_name(const char *mod_path, const char *alias) {
  if (alias && *alias) return strdup(alias);
  char *base = fs_basename(mod_path);
  if (!base) return strdup("Module");
  /* remove .cord */
  size_t n = strlen(base);
  if (n > 5 && strcmp(base + n - 5, ".cord") == 0) {
    base[n - 5] = '\0';
  }
  /* default → DefaultLayout for layouts folder hint */
  if (strcmp(base, "default") == 0) {
    free(base);
    return strdup("DefaultLayout");
  }
  /* PascalCase-ish: ensure first letter upper if letter */
  if (base[0] && islower((unsigned char)base[0])) {
    base[0] = (char)toupper((unsigned char)base[0]);
  }
  return base;
}

int path_is_module_ref(const char *s) {
  if (!s || !*s) return 0;
  return strchr(s, '/') != NULL || strchr(s, '\\') != NULL ||
         strchr(s, '.') != NULL;
}

static char *path_collapse(const char *path) {
  if (!path) return NULL;
  char *tmp = strdup(path);
  if (!tmp) return NULL;
  for (char *p = tmp; *p; p++) {
    if (*p == '\\') *p = '/';
  }

  int is_abs = (tmp[0] == '/');
  char *stack[128];
  int n = 0;
  char *tok = strtok(tmp, "/");
  while (tok) {
    if (strcmp(tok, ".") == 0) {
      /* skip */
    } else if (strcmp(tok, "..") == 0) {
      if (n > 0) n--;
    } else if (tok[0] != '\0') {
      if (n < 128) stack[n++] = tok;
    }
    tok = strtok(NULL, "/");
  }

  size_t cap = strlen(path) + 4;
  char *out = malloc(cap);
  if (!out) {
    free(tmp);
    return NULL;
  }
  size_t o = 0;
  if (is_abs) out[o++] = '/';
  for (int i = 0; i < n; i++) {
    if (i > 0) out[o++] = '/';
    size_t tl = strlen(stack[i]);
    memcpy(out + o, stack[i], tl);
    o += tl;
  }
  if (n == 0 && !is_abs) {
    out[0] = '.';
    out[1] = '\0';
  } else {
    out[o] = '\0';
  }
  free(tmp);
  return out;
}

char *path_make_abs(const char *path) {
  if (!path) return NULL;
  int is_abs = (path[0] == '/');
#ifdef _WIN32
  if (path[0] && path[1] == ':') is_abs = 1;
#endif
  if (is_abs) return path_collapse(path);

  char *cwd = fs_cwd();
  if (!cwd) return path_collapse(path);
  size_t lc = strlen(cwd), lp = strlen(path);
  char *joined = malloc(lc + lp + 2);
  if (!joined) {
    free(cwd);
    return path_collapse(path);
  }
  memcpy(joined, cwd, lc);
  joined[lc] = '/';
  memcpy(joined + lc + 1, path, lp + 1);
  free(cwd);
  char *out = path_collapse(joined);
  free(joined);
  return out;
}

int path_is_under_root(const char *path, const char *root) {
  if (!path || !root || !*root) return 0;
  char *ap = path_make_abs(path);
  char *ar = path_make_abs(root);
  if (!ap || !ar) {
    free(ap);
    free(ar);
    return 0;
  }
  size_t rl = strlen(ar);
  int ok = 0;
  if (strncmp(ap, ar, rl) == 0) {
    if (ap[rl] == '\0' || ap[rl] == '/') ok = 1;
  }
  free(ap);
  free(ar);
  return ok;
}

char *find_project_root(const char *entry_path) {
  char *start = fs_dirname(entry_path);
  char *cur = path_make_abs(start);
  free(start);
  if (!cur) return NULL;
  char *fallback = strdup(cur);
  for (int depth = 0; depth < 48; depth++) {
    size_t n = strlen(cur);
    char *cfg = malloc(n + 20);
    if (!cfg) break;
    memcpy(cfg, cur, n);
    memcpy(cfg + n, "/cordlang.json", 15);
    int found = fs_exists(cfg);
    free(cfg);
    if (found) {
      free(fallback);
      return cur;
    }
    if (strcmp(cur, "/") == 0) break;
#ifdef _WIN32
    if (strlen(cur) <= 3 && cur[1] == ':') break;
#endif
    char *parent = fs_dirname(cur);
    char *next = path_make_abs(parent);
    free(parent);
    if (!next || strcmp(next, cur) == 0) {
      free(next);
      break;
    }
    free(cur);
    cur = next;
  }
  free(cur);
  return fallback;
}

char *resolve_module_file(const char *from_file, const char *mod_path,
                                 const char *project_root) {
  if (!mod_path || !*mod_path) return NULL;

  char *dir = fs_dirname(from_file);
  if (!dir) return NULL;

  /* join dir + mod_path with / */
  size_t ld = strlen(dir), lm = strlen(mod_path);
  char *joined = malloc(ld + lm + 2);
  if (!joined) {
    free(dir);
    return NULL;
  }
  memcpy(joined, dir, ld);
  joined[ld] = '/';
  memcpy(joined + ld + 1, mod_path, lm + 1);
  free(dir);

  char *collapsed = path_collapse(joined);
  free(joined);
  if (!collapsed) return NULL;

  size_t n = strlen(collapsed);
  int has_ext = (n > 5 && strcmp(collapsed + n - 5, ".cord") == 0);

  char *candidate = NULL;
  if (fs_exists(collapsed) && !fs_is_dir(collapsed)) {
    candidate = collapsed;
    collapsed = NULL;
  } else if (!has_ext) {
    char *with_ext = malloc(n + 6);
    if (with_ext) {
      memcpy(with_ext, collapsed, n);
      memcpy(with_ext + n, ".cord", 6);
      if (fs_exists(with_ext) && !fs_is_dir(with_ext)) {
        candidate = with_ext;
      } else {
        free(with_ext);
      }
    }
  }
  free(collapsed);

  if (!candidate) return NULL;

  /* Jail: module must stay under project root */
  if (project_root && !path_is_under_root(candidate, project_root)) {
    free(candidate);
    return NULL;
  }
  return candidate;
}
