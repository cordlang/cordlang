#include "adapters/outbound/fs/fs_internal.h"
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#define PATH_SEP '\\'
#else
#include <unistd.h>
#define PATH_SEP '/'
#endif

char *fs_join(const char *a, const char *b) {
  size_t la = strlen(a);
  size_t lb = strlen(b);
  char *out = malloc(la + lb + 2);
  if (!out) return NULL;
  memcpy(out, a, la);
  if (la > 0 && a[la - 1] != '/' && a[la - 1] != '\\') {
    out[la] = PATH_SEP;
    memcpy(out + la + 1, b, lb + 1);
  } else {
    memcpy(out + la, b, lb + 1);
  }
  return out;
}
char *fs_cwd(void) {
  if (fs_memory_is_mounted()) return fs_memory_current_cwd();
  char buf[4096];
#ifdef _WIN32
  if (!_getcwd(buf, sizeof(buf))) return NULL;
#else
  if (!getcwd(buf, sizeof(buf))) return NULL;
#endif
  return strdup(buf);
}
char *fs_dirname(const char *path) {
  if (!path || !*path) return strdup(".");
  char *copy = strdup(path);
  if (!copy) return NULL;
  size_t len = strlen(copy);
  while (len > 0 && (copy[len - 1] == '/' || copy[len - 1] == '\\')) {
    copy[--len] = '\0';
  }
  char *slash = NULL;
  for (char *p = copy; *p; p++) {
    if (*p == '/' || *p == '\\') slash = p;
  }
  if (!slash) {
    free(copy);
    return strdup(".");
  }
  *slash = '\0';
  if (copy[0] == '\0') {
    free(copy);
    return strdup("/");
  }
  return copy;
}
char *fs_basename(const char *path) {
  if (!path || !*path) return strdup("");
  const char *base = path;
  for (const char *p = path; *p; p++) {
    if (*p == '/' || *p == '\\') base = p + 1;
  }
  return strdup(base);
}
char *fs_norm_path(const char *path) {
  if (fs_memory_is_mounted()) return fs_memory_norm_path(path);
  if (!path) return NULL;
  size_t len = strlen(path);
  char *out = malloc(len + 1);
  if (!out) return NULL;
  size_t o = 0;
  for (size_t i = 0; i < len; i++) {
    char c = path[i];
    if (c == '\\') c = '/';
    /* skip ./ segments */
    if (c == '.' && (i + 1 < len) && (path[i + 1] == '/' || path[i + 1] == '\\')) {
      i++;
      continue;
    }
    if (c == '.' && i + 1 == len) continue;
    out[o++] = c;
  }
  out[o] = '\0';
  return out;
}
