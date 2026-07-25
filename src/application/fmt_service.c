#include "application/fmt_service.h"
#include "application/ports/fs_port.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

/* ── text normalize (safe, file-level) ─────────────────── */

/* Convert tabs → 2 spaces, trim trailing WS per line, collapse >2 blank
 * lines to 1, ensure final newline. Caller frees. */
char *fmt_service_normalize(const char *src, size_t src_len) {
  if (!src) src = "";
  /* worst case: every tab becomes 2 chars + final \n */
  size_t cap = src_len * 2 + 4;
  char *out = malloc(cap);
  if (!out) return NULL;

  size_t o = 0;
  int blank_run = 0;
  size_t i = 0;

  while (i < src_len || (src_len == 0 && i == 0)) {
    if (src_len == 0) break;

    /* gather one logical line (without EOL) into linebuf */
    char linebuf[8192];
    size_t ll = 0;
    while (i < src_len && src[i] != '\n' && src[i] != '\r') {
      char c = src[i++];
      if (c == '\t') {
        if (ll + 2 < sizeof(linebuf)) {
          linebuf[ll++] = ' ';
          linebuf[ll++] = ' ';
        }
      } else if (c != '\0') {
        if (ll + 1 < sizeof(linebuf)) linebuf[ll++] = c;
      }
    }
    /* consume EOL */
    if (i < src_len && src[i] == '\r') i++;
    if (i < src_len && src[i] == '\n') i++;

    /* trim trailing whitespace */
    while (ll > 0 && (linebuf[ll - 1] == ' ' || linebuf[ll - 1] == '\t'))
      ll--;
    linebuf[ll] = '\0';

    int is_blank = (ll == 0);
    if (is_blank) {
      blank_run++;
      if (blank_run > 1) {
        /* collapse: skip extra blank lines (allow at most one) */
        if (i >= src_len) break;
        continue;
      }
    } else {
      blank_run = 0;
    }

    /* ensure capacity */
    if (o + ll + 2 >= cap) {
      cap = (o + ll + 2) * 2;
      char *nbuf = realloc(out, cap);
      if (!nbuf) {
        free(out);
        return NULL;
      }
      out = nbuf;
    }
    if (ll > 0) {
      memcpy(out + o, linebuf, ll);
      o += ll;
    }
    out[o++] = '\n';

    if (i >= src_len) break;
  }

  /* empty file → single newline */
  if (o == 0) {
    out[o++] = '\n';
  }

  /* strip trailing blank lines beyond the required final newline:
   * ensure exactly one trailing newline (file ends with \n, no extra blanks) */
  while (o > 1 && out[o - 1] == '\n' && out[o - 2] == '\n') o--;

  out[o] = '\0';
  return out;
}

static int ends_with_cord(const char *name) {
  if (!name) return 0;
  size_t n = strlen(name);
  return n >= 5 && strcmp(name + n - 5, ".cord") == 0;
}

static int fmt_one_file(const char *path, int write_in_place, int check_only,
                        int *changed_out) {
  size_t len = 0;
  char *src = fs_read_file(path, &len);
  if (!src) {
    fprintf(stderr, "Error: cannot read '%s'\n", path);
    return 1;
  }

  char *formatted = fmt_service_normalize(src, len);
  if (!formatted) {
    free(src);
    fprintf(stderr, "Error: out of memory formatting '%s'\n", path);
    return 1;
  }

  int changed = (strcmp(src, formatted) != 0);
  if (changed_out) *changed_out = changed;

  if (check_only) {
    if (changed) {
      fprintf(stderr, "would reformat: %s\n", path);
      free(src);
      free(formatted);
      return 1;
    }
    free(src);
    free(formatted);
    return 0;
  }

  if (write_in_place) {
    if (changed) {
      if (fs_write_file(path, formatted) != 0) {
        fprintf(stderr, "Error: cannot write '%s'\n", path);
        free(src);
        free(formatted);
        return 1;
      }
      printf("formatted %s\n", path);
    }
  } else {
    fputs(formatted, stdout);
  }

  free(src);
  free(formatted);
  return 0;
}

/* Recursively format .cord under dir. */
static int fmt_walk_dir(const char *dir, int write_in_place, int check_only,
                        int *any_would_change) {
  int rc = 0;

#ifdef _WIN32
  char pattern[MAX_PATH];
  snprintf(pattern, sizeof(pattern), "%s\\*", dir);
  WIN32_FIND_DATAA fd;
  HANDLE h = FindFirstFileA(pattern, &fd);
  if (h == INVALID_HANDLE_VALUE) return 0;

  do {
    if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
      continue;
    char *child = fs_join(dir, fd.cFileName);
    if (!child) continue;

    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      /* skip node_modules / dist / .git */
      if (strcmp(fd.cFileName, "node_modules") != 0 &&
          strcmp(fd.cFileName, "dist") != 0 &&
          strcmp(fd.cFileName, ".git") != 0) {
        int sub = fmt_walk_dir(child, write_in_place, check_only,
                               any_would_change);
        if (sub != 0) rc = sub;
      }
    } else if (ends_with_cord(fd.cFileName)) {
      int changed = 0;
      int fr = fmt_one_file(child, write_in_place, check_only, &changed);
      if (changed && any_would_change) *any_would_change = 1;
      if (fr != 0) rc = fr;
    }
    free(child);
  } while (FindNextFileA(h, &fd));
  FindClose(h);
#else
  DIR *d = opendir(dir);
  if (!d) return 0;
  struct dirent *ent;
  while ((ent = readdir(d)) != NULL) {
    if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
      continue;
    char *child = fs_join(dir, ent->d_name);
    if (!child) continue;

    if (fs_is_dir(child)) {
      if (strcmp(ent->d_name, "node_modules") != 0 &&
          strcmp(ent->d_name, "dist") != 0 &&
          strcmp(ent->d_name, ".git") != 0) {
        int sub =
            fmt_walk_dir(child, write_in_place, check_only, any_would_change);
        if (sub != 0) rc = sub;
      }
    } else if (ends_with_cord(ent->d_name)) {
      int changed = 0;
      int fr = fmt_one_file(child, write_in_place, check_only, &changed);
      if (changed && any_would_change) *any_would_change = 1;
      if (fr != 0) rc = fr;
    }
    free(child);
  }
  closedir(d);
#endif

  return rc;
}

int fmt_service_run(const char *path, int write_in_place, int check_only) {
  const char *p = path && *path ? path : ".";

  if (!fs_exists(p)) {
    fprintf(stderr, "Error: path not found: %s\n", p);
    return 1;
  }

  if (fs_is_dir(p)) {
    int any = 0;
    int rc = fmt_walk_dir(p, write_in_place, check_only, &any);
    if (check_only && any) return 1;
    return rc;
  }

  if (!ends_with_cord(p)) {
    fprintf(stderr, "Error: expected a .cord file or directory, got '%s'\n", p);
    return 1;
  }

  int changed = 0;
  int rc = fmt_one_file(p, write_in_place, check_only, &changed);
  if (check_only && changed) return 1;
  return rc;
}
