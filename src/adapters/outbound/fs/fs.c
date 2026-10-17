#include "adapters/outbound/fs/fs_internal.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define mkdir_one(p) _mkdir(p)
#else
#define mkdir_one(p) mkdir(p, 0755)
#endif

char *fs_read_file(const char *path, size_t *out_len) {
  if (fs_memory_is_mounted()) return fs_memory_read_copy(path, out_len);
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
  if (fs_memory_is_mounted()) return -1;
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
  if (fs_memory_is_mounted()) return -1;
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
  if (fs_memory_is_mounted()) return fs_memory_path_exists(path);
  struct stat st;
  return stat(path, &st) == 0;
}
int fs_is_dir(const char *path) {
  if (fs_memory_is_mounted()) return fs_memory_path_is_dir(path);
  struct stat st;
  if (stat(path, &st) != 0) return 0;
#ifdef _WIN32
  return (st.st_mode & _S_IFDIR) != 0;
#else
  return S_ISDIR(st.st_mode);
#endif
}
