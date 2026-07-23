#include "application/watch_service.h"
#include "application/ports/fs_port.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dirent.h>
#include <signal.h>
#include <unistd.h>
#endif

/* ---- interrupt flag ---- */

static volatile int g_watch_running = 1;

#ifdef _WIN32
static BOOL WINAPI watch_console_ctrl(DWORD type) {
  if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT ||
      type == CTRL_CLOSE_EVENT) {
    g_watch_running = 0;
    return TRUE;
  }
  return FALSE;
}
#else
static void watch_on_sigint(int sig) {
  (void)sig;
  g_watch_running = 0;
}
#endif

static void watch_sleep_ms(int ms) {
#ifdef _WIN32
  Sleep((DWORD)ms);
#else
  usleep((useconds_t)ms * 1000);
#endif
}

/* ---- watched file set ---- */

typedef struct {
  char *path;
  time_t mtime;
} WatchedFile;

typedef struct {
  WatchedFile *files;
  size_t count;
  size_t cap;
} WatchSet;

static void watchset_init(WatchSet *s) {
  s->files = NULL;
  s->count = 0;
  s->cap = 0;
}

static void watchset_free(WatchSet *s) {
  if (!s) return;
  for (size_t i = 0; i < s->count; i++) free(s->files[i].path);
  free(s->files);
  s->files = NULL;
  s->count = 0;
  s->cap = 0;
}

static int watchset_add(WatchSet *s, const char *path, time_t mtime) {
  if (s->count >= s->cap) {
    size_t ncap = s->cap ? s->cap * 2 : 32;
    WatchedFile *nf = realloc(s->files, ncap * sizeof(WatchedFile));
    if (!nf) return -1;
    s->files = nf;
    s->cap = ncap;
  }
  s->files[s->count].path = strdup(path);
  if (!s->files[s->count].path) return -1;
  s->files[s->count].mtime = mtime;
  s->count++;
  return 0;
}

static int path_cmp_files(const void *a, const void *b) {
  const WatchedFile *fa = (const WatchedFile *)a;
  const WatchedFile *fb = (const WatchedFile *)b;
  return strcmp(fa->path, fb->path);
}

static int watchset_equal(const WatchSet *a, const WatchSet *b) {
  if (a->count != b->count) return 0;
  for (size_t i = 0; i < a->count; i++) {
    if (a->files[i].mtime != b->files[i].mtime) return 0;
    if (strcmp(a->files[i].path, b->files[i].path) != 0) return 0;
  }
  return 1;
}

static int ends_with_ci(const char *path, const char *suffix) {
  size_t np = strlen(path);
  size_t ns = strlen(suffix);
  if (np < ns) return 0;
  const char *p = path + (np - ns);
  for (size_t i = 0; i < ns; i++) {
    char a = p[i];
    char b = suffix[i];
    if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
    if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
    if (a != b) return 0;
  }
  return 1;
}

static time_t file_mtime(const char *path) {
  struct stat st;
  if (stat(path, &st) != 0) return 0;
  return st.st_mtime;
}

/* ---- recursive directory walk ---- */

static void scan_file_if_cord(const char *path, WatchSet *set) {
  if (!ends_with_ci(path, ".cord")) return;
  time_t mt = file_mtime(path);
  if (mt == 0 && !fs_exists(path)) return;
  watchset_add(set, path, mt);
}

#ifdef _WIN32
static void walk_dir(const char *dir, WatchSet *set) {
  char pattern[4096];
  snprintf(pattern, sizeof(pattern), "%s\\*", dir);

  WIN32_FIND_DATAA fd;
  HANDLE h = FindFirstFileA(pattern, &fd);
  if (h == INVALID_HANDLE_VALUE) return;

  do {
    if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
      continue;

    char path[4096];
    snprintf(path, sizeof(path), "%s\\%s", dir, fd.cFileName);

    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      walk_dir(path, set);
    } else {
      scan_file_if_cord(path, set);
    }
  } while (FindNextFileA(h, &fd));

  FindClose(h);
}
#else
static void walk_dir(const char *dir, WatchSet *set) {
  DIR *d = opendir(dir);
  if (!d) return;

  struct dirent *ent;
  while ((ent = readdir(d)) != NULL) {
    if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
      continue;

    char path[4096];
    snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name);

    if (fs_is_dir(path)) {
      walk_dir(path, set);
    } else {
      scan_file_if_cord(path, set);
    }
  }
  closedir(d);
}
#endif

static void scan_project(const char *project_dir, WatchSet *set) {
  watchset_free(set);
  watchset_init(set);

  char *src = fs_join(project_dir, "src");
  if (src) {
    if (fs_is_dir(src)) walk_dir(src, set);
    free(src);
  }

  char *cfg = fs_join(project_dir, "cordlang.json");
  if (cfg) {
    if (fs_exists(cfg)) {
      time_t mt = file_mtime(cfg);
      watchset_add(set, cfg, mt);
    }
    free(cfg);
  }

  if (set->count > 1)
    qsort(set->files, set->count, sizeof(WatchedFile), path_cmp_files);
}

/* ---- public API ---- */

int watch_service_run(const char *project_dir, WatchRebuildFn rebuild,
                      void *userdata) {
  const char *dir = project_dir && *project_dir ? project_dir : ".";
  if (!rebuild) {
    fprintf(stderr, "Error: watch: missing rebuild callback\n");
    return 1;
  }

  g_watch_running = 1;

#ifdef _WIN32
  SetConsoleCtrlHandler(watch_console_ctrl, TRUE);
#else
  signal(SIGINT, watch_on_sigint);
  signal(SIGTERM, watch_on_sigint);
#endif

  WatchSet prev, cur;
  watchset_init(&prev);
  watchset_init(&cur);

  scan_project(dir, &prev);

  printf("watching src/**/*.cord (Ctrl+C to stop)...\n"); /* glob-style message */
  fflush(stdout);

  while (g_watch_running) {
    watch_sleep_ms(500);
    if (!g_watch_running) break;

    scan_project(dir, &cur);
    if (!watchset_equal(&prev, &cur)) {
      /* Debounce: wait briefly and rescan so partial saves settle. */
      watch_sleep_ms(150);
      if (!g_watch_running) break;
      scan_project(dir, &cur);

      printf("\nchange detected, rebuilding...\n");
      fflush(stdout);
      (void)rebuild(userdata);

      watchset_free(&prev);
      prev = cur;
      watchset_init(&cur);
    }
  }

  watchset_free(&prev);
  watchset_free(&cur);

  printf("\nwatch stopped.\n");
  fflush(stdout);

#ifdef _WIN32
  SetConsoleCtrlHandler(watch_console_ctrl, FALSE);
#endif

  return 0;
}
