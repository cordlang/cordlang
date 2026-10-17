#include "adapters/outbound/fs/fs_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define mkdir_one(path) _mkdir(path)
#else
#include <dirent.h>
#include <sys/stat.h>
#define mkdir_one(path) mkdir(path, 0755)
#endif

int fs_copy_file(const char *src, const char *dst) {
  if (fs_memory_is_mounted()) return -1;
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
  if (fs_memory_is_mounted()) return -1;
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
int fs_copy_tree(const char *src, const char *dst) {
  if (fs_memory_is_mounted()) return -1;
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
