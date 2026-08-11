#include "application/ports/fs_port.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define PATH_SEP '\\'
#define mkdir_one(p) _mkdir(p)
#else
#include <unistd.h>
#define PATH_SEP '/'
#define mkdir_one(p) mkdir(p, 0755)
#endif

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

char *fs_read_file(const char *path, size_t *out_len) {
  if (g_memory_mounted) {
    MountedMemoryFile *file = memory_find_file(path);
    if (!file) return NULL;
    char *copy = malloc(file->len + 1);
    if (!copy) return NULL;
    memcpy(copy, file->data, file->len + 1);
    if (out_len) *out_len = file->len;
    return copy;
  }
  FILE *f = fopen(path, "rb");
  if (!f) return NULL;

  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return NULL;
  }
  long size = ftell(f);
  if (size < 0) {
    fclose(f);
    return NULL;
  }
  rewind(f);

  char *buf = malloc((size_t)size + 1);
  if (!buf) {
    fclose(f);
    return NULL;
  }
  size_t n = fread(buf, 1, (size_t)size, f);
  fclose(f);
  buf[n] = '\0';
  if (out_len) *out_len = n;
  return buf;
}

int fs_write_file(const char *path, const char *content) {
  if (g_memory_mounted) return -1;
  /* Ensure parent directories exist */
  char *copy = strdup(path);
  if (!copy) return -1;

  for (char *p = copy + 1; *p; p++) {
    if (*p == '/' || *p == '\\') {
      char saved = *p;
      *p = '\0';
      mkdir_one(copy);
      *p = saved;
    }
  }
  free(copy);

  FILE *f = fopen(path, "wb");
  if (!f) return -1;
  size_t len = strlen(content);
  size_t written = fwrite(content, 1, len, f);
  fclose(f);
  return written == len ? 0 : -1;
}

int fs_mkdir_p(const char *path) {
  if (g_memory_mounted) return -1;
  if (!path || !*path) return -1;
  char *copy = strdup(path);
  if (!copy) return -1;

  size_t len = strlen(copy);
  while (len > 0 && (copy[len - 1] == '/' || copy[len - 1] == '\\')) {
    copy[--len] = '\0';
  }

  for (char *p = copy + 1; *p; p++) {
    if (*p == '/' || *p == '\\') {
      char saved = *p;
      *p = '\0';
      if (mkdir_one(copy) != 0 && errno != EEXIST) {
        /* ignore if exists */
      }
      *p = saved;
    }
  }
  if (mkdir_one(copy) != 0 && errno != EEXIST) {
    free(copy);
    return -1;
  }
  free(copy);
  return 0;
}

int fs_exists(const char *path) {
  if (g_memory_mounted)
    return memory_find_file(path) != NULL || memory_is_dir(path);
  struct stat st;
  return stat(path, &st) == 0;
}

int fs_is_dir(const char *path) {
  if (g_memory_mounted) return memory_is_dir(path);
  struct stat st;
  if (stat(path, &st) != 0) return 0;
#ifdef _WIN32
  return (st.st_mode & _S_IFDIR) != 0;
#else
  return S_ISDIR(st.st_mode);
#endif
}

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
  if (g_memory_mounted) return strdup(g_memory_cwd);
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
  if (g_memory_mounted) return memory_normalize_path(path, g_memory_cwd);
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

int fs_copy_file(const char *src, const char *dst) {
  if (g_memory_mounted) return -1;
  if (!src || !dst) return -1;
  size_t len = 0;
  char *data = fs_read_file(src, &len);
  if (!data) return -1;

  /* Ensure parent directories exist (fs_write_file does this for text;
   * we need binary-safe write with exact length). */
  char *copy = strdup(dst);
  if (!copy) {
    free(data);
    return -1;
  }
  for (char *p = copy + 1; *p; p++) {
    if (*p == '/' || *p == '\\') {
      char saved = *p;
      *p = '\0';
      mkdir_one(copy);
      *p = saved;
    }
  }
  free(copy);

  FILE *f = fopen(dst, "wb");
  if (!f) {
    free(data);
    return -1;
  }
  size_t written = fwrite(data, 1, len, f);
  fclose(f);
  free(data);
  return written == len ? 0 : -1;
}

#ifdef _WIN32
int fs_copy_tree(const char *src, const char *dst) {
  if (g_memory_mounted) return -1;
  if (!src || !dst) return -1;
  if (!fs_is_dir(src)) return -1;
  if (fs_mkdir_p(dst) != 0) return -1;

  char pattern[MAX_PATH];
  snprintf(pattern, sizeof(pattern), "%s\\*", src);

  WIN32_FIND_DATAA fd;
  HANDLE h = FindFirstFileA(pattern, &fd);
  if (h == INVALID_HANDLE_VALUE) {
    /* empty or unreadable — empty dst is still ok */
    return 0;
  }

  int rc = 0;
  do {
    if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
      continue;
    char *src_child = fs_join(src, fd.cFileName);
    char *dst_child = fs_join(dst, fd.cFileName);
    if (!src_child || !dst_child) {
      free(src_child);
      free(dst_child);
      rc = -1;
      break;
    }
    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      if (fs_copy_tree(src_child, dst_child) != 0) rc = -1;
    } else {
      if (fs_copy_file(src_child, dst_child) != 0) rc = -1;
    }
    free(src_child);
    free(dst_child);
    if (rc != 0) break;
  } while (FindNextFileA(h, &fd));

  FindClose(h);
  return rc;
}
#else
#include <dirent.h>

int fs_copy_tree(const char *src, const char *dst) {
  if (g_memory_mounted) return -1;
  if (!src || !dst) return -1;
  if (!fs_is_dir(src)) return -1;
  if (fs_mkdir_p(dst) != 0) return -1;

  DIR *d = opendir(src);
  if (!d) return 0; /* empty/unreadable → ok empty dst */

  int rc = 0;
  struct dirent *ent;
  while ((ent = readdir(d)) != NULL) {
    if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
      continue;
    char *src_child = fs_join(src, ent->d_name);
    char *dst_child = fs_join(dst, ent->d_name);
    if (!src_child || !dst_child) {
      free(src_child);
      free(dst_child);
      rc = -1;
      break;
    }
    if (fs_is_dir(src_child)) {
      if (fs_copy_tree(src_child, dst_child) != 0) rc = -1;
    } else {
      if (fs_copy_file(src_child, dst_child) != 0) rc = -1;
    }
    free(src_child);
    free(dst_child);
    if (rc != 0) break;
  }
  closedir(d);
  return rc;
}
#endif

#ifdef _WIN32
static void walk_cord_rec(const char *root, const char *dir, int depth,
                          void (*cb)(const char *abs, const char *rel, void *ud),
                          void *ud) {
  if (depth > 16 || !root || !dir || !cb) return;
  char pattern[4096];
  snprintf(pattern, sizeof(pattern), "%s\\*", dir);
  WIN32_FIND_DATAA fd;
  HANDLE h = FindFirstFileA(pattern, &fd);
  if (h == INVALID_HANDLE_VALUE) return;
  do {
    if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0 ||
        strcmp(fd.cFileName, "node_modules") == 0 ||
        strcmp(fd.cFileName, ".git") == 0 || strcmp(fd.cFileName, "dist") == 0)
      continue;
    char path[4096];
    snprintf(path, sizeof(path), "%s\\%s", dir, fd.cFileName);
    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      walk_cord_rec(root, path, depth + 1, cb, ud);
    } else {
      size_t n = strlen(fd.cFileName);
      if (n > 5 && (_stricmp(fd.cFileName + n - 5, ".cord") == 0)) {
        char rel[512];
        size_t rl = strlen(root);
        const char *p = path;
        if (strncmp(path, root, rl) == 0) {
          p = path + rl;
          while (*p == '\\' || *p == '/') p++;
        }
        size_t i = 0;
        for (; p[i] && i + 1 < sizeof(rel); i++)
          rel[i] = (p[i] == '\\') ? '/' : p[i];
        rel[i] = '\0';
        cb(path, rel, ud);
      }
    }
  } while (FindNextFileA(h, &fd));
  FindClose(h);
}
#else
static void walk_cord_rec(const char *root, const char *dir, int depth,
                          void (*cb)(const char *abs, const char *rel, void *ud),
                          void *ud) {
  if (depth > 16 || !root || !dir || !cb) return;
  DIR *d = opendir(dir);
  if (!d) return;
  struct dirent *ent;
  while ((ent = readdir(d)) != NULL) {
    if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0 ||
        strcmp(ent->d_name, "node_modules") == 0 ||
        strcmp(ent->d_name, ".git") == 0 || strcmp(ent->d_name, "dist") == 0)
      continue;
    char path[4096];
    snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name);
    struct stat sb;
    if (stat(path, &sb) != 0) continue;
    if (S_ISDIR(sb.st_mode)) {
      walk_cord_rec(root, path, depth + 1, cb, ud);
    } else {
      size_t n = strlen(ent->d_name);
      if (n > 5 && strcmp(ent->d_name + n - 5, ".cord") == 0) {
        char rel[512];
        size_t rl = strlen(root);
        const char *p = path;
        if (strncmp(path, root, rl) == 0) {
          p = path + rl;
          while (*p == '/') p++;
        }
        snprintf(rel, sizeof(rel), "%s", p);
        cb(path, rel, ud);
      }
    }
  }
  closedir(d);
}
#endif

void fs_walk_cord(const char *root,
                  void (*cb)(const char *abs, const char *rel, void *ud),
                  void *ud) {
  if (g_memory_mounted) return;
  if (!root || !cb) return;
  walk_cord_rec(root, root, 0, cb, ud);
}
