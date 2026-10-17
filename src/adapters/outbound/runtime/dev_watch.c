/* Feature-test macros must precede system headers (glibc / POSIX). */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "adapters/outbound/runtime/dev_watch_internal.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dirent.h>
#endif
/* ── change detection ───────────────────────────────────── */

/*
 * Fingerprint of every .cord under the project, every file under public/, and
 * cordlang.json. A file list lets soft HMR know whether a single leaf .cord
 * changed (update) vs entry / public / config (full reload).
 */
static void path_to_rel(const char *root, const char *abs, char *out, size_t n) {
  size_t rl = root ? strlen(root) : 0;
  const char *p = abs;
  if (root && strncmp(abs, root, rl) == 0) {
    p = abs + rl;
    while (*p == '/' || *p == '\\') p++;
  }
  size_t i = 0;
  for (; p[i] && i + 1 < n; i++)
    out[i] = (p[i] == '\\') ? '/' : p[i];
  out[i] = '\0';
}

static unsigned long long file_sig(const char *path, time_t mtime, long long size) {
  unsigned long long h = 14695981039346656037ULL;
  for (const char *p = path; *p; p++) {
    char c = *p;
    if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    if (c == '\\') c = '/';
    h = h * 1099511628211ULL ^ (unsigned long long)(unsigned char)c;
  }
  h = h * 1099511628211ULL ^ (unsigned long long)mtime;
  h = h * 1099511628211ULL ^ (unsigned long long)size;
  return h;
}

static void stamp_add(WatchSnapshot *snap, const char *root, const char *abs,
                      time_t mtime, long long size) {
  unsigned long long sig = file_sig(abs, mtime, size);
  snap->stamp.hash ^= sig + (unsigned long long)snap->stamp.files * 0x9e3779b97f4a7c15ULL;
  snap->stamp.files++;
  if (snap->n_files < MAX_WATCH_FILES) {
    WatchFile *f = &snap->files[snap->n_files++];
    path_to_rel(root, abs, f->rel, sizeof(f->rel));
    f->sig = sig;
  }
}

static int is_cord_file(const char *name) {
  size_t n = strlen(name);
  if (n < 6) return 0;
  return strcmp(name + n - 5, ".cord") == 0 || strcmp(name + n - 5, ".CORD") == 0;
}

static int skip_dir(const char *name) {
  return strcmp(name, ".") == 0 || strcmp(name, "..") == 0 ||
         strcmp(name, "node_modules") == 0 || strcmp(name, ".git") == 0 ||
         strcmp(name, "dist") == 0;
}

static int basename_is(const char *path, const char *name) {
  const char *base = path;
  for (const char *p = path; *p; p++)
    if (*p == '/' || *p == '\\') base = p + 1;
  return strcmp(base, name) == 0;
}

#ifdef _WIN32
static void walk_stamp(const char *root, const char *dir, WatchSnapshot *snap,
                       int depth, int in_public) {
  if (depth > 16) return;
  char pattern[4096];
  snprintf(pattern, sizeof(pattern), "%s\\*", dir);
  WIN32_FIND_DATAA fd;
  HANDLE h = FindFirstFileA(pattern, &fd);
  if (h == INVALID_HANDLE_VALUE) return;
  do {
    if (skip_dir(fd.cFileName)) continue;
    char path[4096];
    snprintf(path, sizeof(path), "%s\\%s", dir, fd.cFileName);
    int next_pub = in_public || basename_is(path, "public");
    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      walk_stamp(root, path, snap, depth + 1, next_pub);
    } else if (is_cord_file(fd.cFileName) || in_public || next_pub) {
      /* Files directly in public/ are stamped when parent is public. */
      if (!(is_cord_file(fd.cFileName) || in_public)) continue;
      ULARGE_INTEGER t;
      t.LowPart = fd.ftLastWriteTime.dwLowDateTime;
      t.HighPart = fd.ftLastWriteTime.dwHighDateTime;
      stamp_add(snap, root, path, (time_t)(t.QuadPart / 10000000ULL),
                (long long)fd.nFileSizeLow);
    }
  } while (FindNextFileA(h, &fd));
  FindClose(h);
}
#else
static void walk_stamp(const char *root, const char *dir, WatchSnapshot *snap,
                       int depth, int in_public) {
  if (depth > 16) return;
  DIR *d = opendir(dir);
  if (!d) return;
  struct dirent *ent;
  while ((ent = readdir(d)) != NULL) {
    if (skip_dir(ent->d_name)) continue;
    char path[4096];
    snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name);
    struct stat sb;
    if (stat(path, &sb) != 0) continue;
    int next_pub = in_public || basename_is(path, "public");
    if (S_ISDIR(sb.st_mode)) {
      walk_stamp(root, path, snap, depth + 1, next_pub);
    } else if (is_cord_file(ent->d_name) || in_public) {
      stamp_add(snap, root, path, sb.st_mtime, (long long)sb.st_size);
    }
  }
  closedir(d);
}
#endif

WatchSnapshot dev_watch_take_snapshot(const char *root) {
  WatchSnapshot snap;
  memset(&snap, 0, sizeof(snap));
  snap.stamp.hash = 14695981039346656037ULL;
  if (!root) return snap;
  walk_stamp(root, root, &snap, 0, 0);
  char cfg[4096];
  snprintf(cfg, sizeof(cfg), "%s/cordlang.json", root);
  struct stat sb;
  if (stat(cfg, &sb) == 0)
    stamp_add(&snap, root, cfg, sb.st_mtime, (long long)sb.st_size);
  return snap;
}

static WatchStamp take_stamp(const char *root) {
  return dev_watch_take_snapshot(root).stamp;
}

unsigned long long dev_server_project_stamp(const char *watch_dir) {
  return take_stamp(watch_dir).hash;
}

int dev_watch_stamp_equal(const WatchStamp *a, const WatchStamp *b) {
  return a->hash == b->hash && a->files == b->files;
}

/*
 * Compare snapshots. Sets *out_url to "/rel" for a single soft-updatable .cord
 * change, or leaves it empty for full reload. Returns 1 if full_reload.
 */
int dev_watch_classify_change(const WatchSnapshot *prev, const WatchSnapshot *now,
                           const char *entry_rel, char *out_url, size_t out_n) {
  if (out_url && out_n) out_url[0] = '\0';
  int changed = 0;
  char only[512];
  only[0] = '\0';

  for (int i = 0; i < now->n_files; i++) {
    const WatchFile *nf = &now->files[i];
    int found = 0;
    unsigned long long old_sig = 0;
    for (int j = 0; j < prev->n_files; j++) {
      if (strcmp(prev->files[j].rel, nf->rel) == 0) {
        found = 1;
        old_sig = prev->files[j].sig;
        break;
      }
    }
    if (!found || old_sig != nf->sig) {
      changed++;
      if (changed == 1) snprintf(only, sizeof(only), "%s", nf->rel);
      else only[0] = '\0';
    }
  }
  for (int j = 0; j < prev->n_files; j++) {
    int found = 0;
    for (int i = 0; i < now->n_files; i++) {
      if (strcmp(now->files[i].rel, prev->files[j].rel) == 0) {
        found = 1;
        break;
      }
    }
    if (!found) {
      changed++;
      only[0] = '\0';
    }
  }

  if (changed != 1 || !only[0]) return 1;

  /* public/ or cordlang.json → full reload */
  if (strncmp(only, "public/", 7) == 0 || strcmp(only, "cordlang.json") == 0)
    return 1;
  size_t n = strlen(only);
  if (n < 6 || strcmp(only + n - 5, ".cord") != 0) return 1;

  /* entry file → full reload (routes/theme) */
  if (entry_rel && *entry_rel) {
    char er[512];
    size_t i = 0;
    const char *e = entry_rel;
    while (*e == '/') e++;
    for (; e[i] && i + 1 < sizeof(er); i++)
      er[i] = (e[i] == '\\') ? '/' : e[i];
    er[i] = '\0';
    if (strcmp(only, er) == 0) return 1;
  }

  if (out_url && out_n) snprintf(out_url, out_n, "/%s", only);
  return 0;
}

