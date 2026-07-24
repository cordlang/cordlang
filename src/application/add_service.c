#include "application/add_service.h"
#include "application/ports/fs_port.h"
#include "adapters/outbound/json/json_mini.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

/* Walk up from start looking for relpath that exists. Caller frees. */
static char *find_up(const char *start, const char *relpath) {
  char *cur = fs_norm_path(start);
  if (!cur) return NULL;
  for (int depth = 0; depth < 12; depth++) {
    char *cand = fs_join(cur, relpath);
    if (cand && fs_exists(cand)) {
      free(cur);
      return cand;
    }
    free(cand);
    char *parent = fs_dirname(cur);
    if (!parent) break;
    if (strcmp(parent, cur) == 0) {
      free(parent);
      break;
    }
    free(cur);
    cur = parent;
  }
  free(cur);
  return NULL;
}

static int dir_has_cord_files(const char *dir) {
#ifdef _WIN32
  char pattern[1024];
  snprintf(pattern, sizeof(pattern), "%s\\*", dir);
  WIN32_FIND_DATAA fd;
  HANDLE h = FindFirstFileA(pattern, &fd);
  if (h == INVALID_HANDLE_VALUE) return 0;
  int found = 0;
  do {
    if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
      continue;
    size_t n = strlen(fd.cFileName);
    if (n > 5 && strcmp(fd.cFileName + n - 5, ".cord") == 0) found = 1;
    if (strcmp(fd.cFileName, "cordlang.pkg.json") == 0) found = 1;
    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      char child[1024];
      snprintf(child, sizeof(child), "%s\\%s", dir, fd.cFileName);
      if (dir_has_cord_files(child)) found = 1;
    }
  } while (FindNextFileA(h, &fd));
  FindClose(h);
  return found;
#else
  DIR *d = opendir(dir);
  if (!d) return 0;
  int found = 0;
  struct dirent *ent;
  while ((ent = readdir(d)) != NULL) {
    if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
      continue;
    size_t n = strlen(ent->d_name);
    if (n > 5 && strcmp(ent->d_name + n - 5, ".cord") == 0) found = 1;
    if (strcmp(ent->d_name, "cordlang.pkg.json") == 0) found = 1;
    char *child = fs_join(dir, ent->d_name);
    if (child && fs_is_dir(child) && dir_has_cord_files(child)) found = 1;
    free(child);
  }
  closedir(d);
  return found;
#endif
}

static char *read_pkg_name(const char *pkg_dir) {
  char *pkg = fs_join(pkg_dir, "cordlang.pkg.json");
  if (!pkg) return NULL;
  char *name = NULL;
  if (fs_exists(pkg)) {
    size_t len = 0;
    char *json = fs_read_file(pkg, &len);
    if (json) {
      name = json_object_get_string(json, "name");
      free(json);
    }
  }
  free(pkg);
  if (!name) {
    /* Fallback: cordlang.json "name" (templates often use this). */
    char *cfg = fs_join(pkg_dir, "cordlang.json");
    if (cfg && fs_exists(cfg)) {
      size_t len = 0;
      char *json = fs_read_file(cfg, &len);
      if (json) {
        name = json_object_get_string(json, "name");
        free(json);
      }
    }
    free(cfg);
  }
  return name;
}

static char *sanitize_pkg_name(const char *raw) {
  if (!raw || !*raw) return strdup("package");
  /* Use final path segment; strip leading @scope/ style to basename. */
  char *base = fs_basename(raw);
  if (!base) return strdup("package");
  /* Prefer last segment after '/' already handled by basename. */
  for (char *p = base; *p; p++) {
    if (*p == ' ' || *p == '\\') *p = '-';
  }
  return base;
}

/* Resolve local package path. Caller frees. */
static char *resolve_package(const char *path_or_name) {
  if (!path_or_name || !*path_or_name) return NULL;

  if (fs_exists(path_or_name) && fs_is_dir(path_or_name)) {
    return fs_norm_path(path_or_name);
  }

  /* Named package: look under cwd, then templates/, then walk up. */
  char *cwd = fs_cwd();
  if (!cwd) return NULL;

  char *direct = fs_join(cwd, path_or_name);
  if (direct && fs_exists(direct) && fs_is_dir(direct)) {
    free(cwd);
    char *out = fs_norm_path(direct);
    free(direct);
    return out;
  }
  free(direct);

  char rel_tpl[512];
  snprintf(rel_tpl, sizeof(rel_tpl), "templates/%s", path_or_name);
  char *tpl = find_up(cwd, rel_tpl);
  free(cwd);
  if (tpl) return tpl;

  return NULL;
}

int add_service_run(const char *path_or_name, int dest_lib) {
  if (!path_or_name || !*path_or_name) {
    fprintf(stderr, "Error: missing package path or name\n");
    fprintf(stderr, "Usage: cordlang add <path-or-name> [--lib]\n");
    return 1;
  }

  char *src = resolve_package(path_or_name);
  if (!src) {
    fprintf(stderr, "Error: package not found: %s\n", path_or_name);
    fprintf(stderr,
            "Hint: pass a folder of .cord files (optional cordlang.pkg.json),\n"
            "      or a template id under templates/ (e.g. counter).\n");
    return 1;
  }

  if (!dir_has_cord_files(src)) {
    fprintf(stderr,
            "Error: '%s' does not look like a Cord package "
            "(.cord files or cordlang.pkg.json)\n",
            src);
    free(src);
    return 1;
  }

  char *pkg_name = read_pkg_name(src);
  if (!pkg_name) {
    char *base = fs_basename(src);
    pkg_name = sanitize_pkg_name(base);
    free(base);
  } else {
    char *clean = sanitize_pkg_name(pkg_name);
    free(pkg_name);
    pkg_name = clean;
  }

  const char *subdir = dest_lib ? "src/lib" : "src/vendor";
  char *dest_parent = fs_join(".", subdir);
  if (!dest_parent) {
    free(src);
    free(pkg_name);
    return 1;
  }
  if (fs_mkdir_p(dest_parent) != 0) {
    fprintf(stderr, "Error: cannot create %s/\n", subdir);
    free(dest_parent);
    free(src);
    free(pkg_name);
    return 1;
  }

  char *dest = fs_join(dest_parent, pkg_name);
  free(dest_parent);
  if (!dest) {
    free(src);
    free(pkg_name);
    return 1;
  }

  if (fs_exists(dest)) {
    fprintf(stderr, "Error: already exists: %s\n", dest);
    free(dest);
    free(src);
    free(pkg_name);
    return 1;
  }

  if (fs_copy_tree(src, dest) != 0) {
    fprintf(stderr, "Error: failed to copy '%s' → '%s'\n", src, dest);
    free(dest);
    free(src);
    free(pkg_name);
    return 1;
  }

  printf("Added package '%s' → %s/\n", pkg_name, dest);
  printf("Use in .cord:  use vendor/%s/...  or  use lib/%s/...\n", pkg_name,
         pkg_name);
  free(dest);
  free(src);
  free(pkg_name);
  return 0;
}
