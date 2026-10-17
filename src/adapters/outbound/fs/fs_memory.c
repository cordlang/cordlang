#include "adapters/outbound/fs/fs_internal.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
  char *path;
  char *data;
  size_t len;
} MountedMemoryFile;

static MountedMemoryFile *g_memory_files;
static size_t g_memory_count;
static char *g_memory_cwd;
static int g_memory_mounted;

static char *memory_normalize_path(const char *path, const char *cwd) {
  if (!path || !*path) return NULL;

  int relative = path[0] != '/';
  size_t lc = relative && cwd ? strlen(cwd) : 0;
  size_t lp = strlen(path);
  char *raw = malloc(lc + lp + 2);
  if (!raw) return NULL;

  size_t o = 0;
  if (relative && cwd) {
    memcpy(raw, cwd, lc);
    o = lc;
    if (o == 0 || raw[o - 1] != '/') raw[o++] = '/';
  }
  for (size_t i = 0; i < lp; i++) raw[o++] = path[i] == '\\' ? '/' : path[i];
  raw[o] = '\0';

  char *parts[256];
  size_t n_parts = 0;
  char *cursor = raw;
  while (*cursor) {
    while (*cursor == '/') cursor++;
    if (!*cursor) break;
    char *part = cursor;
    while (*cursor && *cursor != '/') cursor++;
    if (*cursor) *cursor++ = '\0';
    if (strcmp(part, ".") == 0 || !*part) continue;
    if (strcmp(part, "..") == 0) {
      if (n_parts > 0) n_parts--;
      continue;
    }
    if (n_parts >= sizeof(parts) / sizeof(parts[0])) {
      free(raw);
      return NULL;
    }
    parts[n_parts++] = part;
  }

  size_t out_len = 1;
  for (size_t i = 0; i < n_parts; i++) out_len += strlen(parts[i]) + 1;
  char *out = malloc(out_len + 1);
  if (!out) {
    free(raw);
    return NULL;
  }
  o = 0;
  out[o++] = '/';
  for (size_t i = 0; i < n_parts; i++) {
    if (i > 0) out[o++] = '/';
    size_t plen = strlen(parts[i]);
    memcpy(out + o, parts[i], plen);
    o += plen;
  }
  out[o] = '\0';
  free(raw);
  return out;
}
static void memory_files_free(MountedMemoryFile *files, size_t count) {
  if (!files) return;
  for (size_t i = 0; i < count; i++) {
    free(files[i].path);
    free(files[i].data);
  }
  free(files);
}
int fs_memory_mount(const FsMemoryFile *files, size_t count,
                    const char *virtual_cwd) {
  if (g_memory_mounted || !files || count == 0 || !virtual_cwd ||
      virtual_cwd[0] != '/')
    return -1;

  char *cwd = memory_normalize_path(virtual_cwd, NULL);
  if (!cwd) return -1;
  MountedMemoryFile *mounted = calloc(count, sizeof(*mounted));
  if (!mounted) {
    free(cwd);
    return -1;
  }

  for (size_t i = 0; i < count; i++) {
    if (!files[i].path || files[i].path[0] != '/' || !files[i].data) {
      memory_files_free(mounted, count);
      free(cwd);
      return -1;
    }
    mounted[i].path = memory_normalize_path(files[i].path, cwd);
    if (!mounted[i].path) {
      memory_files_free(mounted, count);
      free(cwd);
      return -1;
    }
    for (size_t j = 0; j < i; j++) {
      if (strcmp(mounted[i].path, mounted[j].path) == 0) {
        memory_files_free(mounted, count);
        free(cwd);
        return -1;
      }
    }
    if (files[i].len == (size_t)-1) {
      memory_files_free(mounted, count);
      free(cwd);
      return -1;
    }
    mounted[i].data = malloc(files[i].len + 1);
    if (!mounted[i].data) {
      memory_files_free(mounted, count);
      free(cwd);
      return -1;
    }
    memcpy(mounted[i].data, files[i].data, files[i].len);
    mounted[i].data[files[i].len] = '\0';
    mounted[i].len = files[i].len;
  }

  g_memory_files = mounted;
  g_memory_count = count;
  g_memory_cwd = cwd;
  g_memory_mounted = 1;
  return 0;
}
void fs_memory_unmount(void) {
  memory_files_free(g_memory_files, g_memory_count);
  free(g_memory_cwd);
  g_memory_files = NULL;
  g_memory_count = 0;
  g_memory_cwd = NULL;
  g_memory_mounted = 0;
}
static MountedMemoryFile *memory_find_file(const char *path) {
  if (!g_memory_mounted || !path) return NULL;
  char *normalized = memory_normalize_path(path, g_memory_cwd);
  if (!normalized) return NULL;
  MountedMemoryFile *found = NULL;
  for (size_t i = 0; i < g_memory_count; i++) {
    if (strcmp(g_memory_files[i].path, normalized) == 0) {
      found = &g_memory_files[i];
      break;
    }
  }
  free(normalized);
  return found;
}
static int memory_is_dir(const char *path) {
  if (!g_memory_mounted || !path) return 0;
  char *normalized = memory_normalize_path(path, g_memory_cwd);
  if (!normalized) return 0;
  size_t len = strlen(normalized);
  int found = 0;
  for (size_t i = 0; i < g_memory_count; i++) {
    const char *candidate = g_memory_files[i].path;
    if (strcmp(candidate, normalized) == 0) break;
    if (strcmp(normalized, "/") == 0 ||
        (strncmp(candidate, normalized, len) == 0 && candidate[len] == '/')) {
      found = 1;
      break;
    }
  }
  free(normalized);
  return found;
}

int fs_memory_is_mounted(void) { return g_memory_mounted; }

char *fs_memory_current_cwd(void) {
  return g_memory_mounted ? strdup(g_memory_cwd) : NULL;
}

char *fs_memory_norm_path(const char *path) {
  return g_memory_mounted ? memory_normalize_path(path, g_memory_cwd) : NULL;
}

char *fs_memory_read_copy(const char *path, size_t *out_len) {
  MountedMemoryFile *file = memory_find_file(path);
  if (!file) return NULL;
  char *copy = malloc(file->len + 1);
  if (!copy) return NULL;
  memcpy(copy, file->data, file->len + 1);
  if (out_len) *out_len = file->len;
  return copy;
}

int fs_memory_path_exists(const char *path) {
  return memory_find_file(path) != NULL || memory_is_dir(path);
}

int fs_memory_path_is_dir(const char *path) { return memory_is_dir(path); }
