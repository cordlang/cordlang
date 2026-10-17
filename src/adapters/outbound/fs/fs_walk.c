#include "adapters/outbound/fs/fs_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
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
  if (fs_memory_is_mounted()) return;
  if (!root || !cb) return;
  walk_cord_rec(root, root, 0, cb, ud);
}

