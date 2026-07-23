#include "application/run_service.h"
#include "application/compile_service.h"
#include "application/watch_service.h"
#include "application/ports/backend_port.h"
#include "application/ports/compiler_port.h"
#include "application/ports/fs_port.h"
#include "domain/ir.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_entry_from_config(const char *project_dir) {
  char *cfg = fs_join(project_dir, "cordlang.json");
  if (!cfg) return NULL;

  size_t len = 0;
  char *json = fs_read_file(cfg, &len);
  free(cfg);
  if (!json) return NULL;

  /* Minimal parse: look for "entry": "..." */
  char *entry = NULL;
  char *key = strstr(json, "\"entry\"");
  if (key) {
    char *colon = strchr(key, ':');
    if (colon) {
      char *q1 = strchr(colon, '"');
      if (q1) {
        q1++;
        char *q2 = strchr(q1, '"');
        if (q2) {
          size_t n = (size_t)(q2 - q1);
          entry = malloc(n + 1);
          memcpy(entry, q1, n);
          entry[n] = '\0';
        }
      }
    }
  }
  free(json);

  if (!entry) entry = strdup("src/app.cord");
  return entry;
}

/* After scaffold: optional npm install + vite build smoke check. */
static int run_vite_check(const char *project_dir, const char *backend_name) {
  char rel[64];
  snprintf(rel, sizeof(rel), "dist/%s", backend_name);
  char *dist = fs_join(project_dir, rel);
  if (!dist) return 1;

  if (!fs_exists(dist)) {
    fprintf(stderr, "Error: --check: dist dir missing: %s\n", dist);
    free(dist);
    return 1;
  }

  char *nm = fs_join(dist, "node_modules");
  int need_install = !nm || !fs_exists(nm);
  free(nm);

  printf("\n--check: verifying build in %s\n", dist);
  fflush(stdout);

  char cmd[2048];
  int rc = 0;

#ifdef _WIN32
  if (need_install) {
    printf("--check: node_modules missing, running npm install...\n");
    fflush(stdout);
    snprintf(cmd, sizeof(cmd), "cmd /c \"cd /d \"%s\" && npm install\"", dist);
    rc = system(cmd);
    if (rc != 0) {
      fprintf(stderr, "Error: --check: npm install failed (exit %d)\n", rc);
      free(dist);
      return 1;
    }
  } else {
    printf("--check: node_modules present, skipping npm install\n");
  }
  printf("--check: running npx vite build...\n");
  fflush(stdout);
  snprintf(cmd, sizeof(cmd), "cmd /c \"cd /d \"%s\" && npx vite build\"", dist);
  rc = system(cmd);
#else
  if (need_install) {
    printf("--check: node_modules missing, running npm install...\n");
    fflush(stdout);
    snprintf(cmd, sizeof(cmd), "cd \"%s\" && npm install", dist);
    rc = system(cmd);
    if (rc != 0) {
      fprintf(stderr, "Error: --check: npm install failed (exit %d)\n", rc);
      free(dist);
      return 1;
    }
  } else {
    printf("--check: node_modules present, skipping npm install\n");
  }
  printf("--check: running npx vite build...\n");
  fflush(stdout);
  snprintf(cmd, sizeof(cmd), "cd \"%s\" && npx vite build", dist);
  rc = system(cmd);
#endif

  free(dist);
  if (rc != 0) {
    fprintf(stderr, "Error: --check: vite build failed (exit %d)\n", rc);
    return 1;
  }
  printf("--check: vite build OK\n");
  return 0;
}

/* Compile entry + scaffold for backend. quiet=1 softens error noise for
 * watch rebuilds (still prints errors, keeps watching). */
static int run_scaffold_once(const BackendPort *backend, const char *dir,
                             int quiet) {
  char *entry_rel = read_entry_from_config(dir);
  char *entry = fs_join(dir, entry_rel);
  free(entry_rel);

  if (!fs_exists(entry)) {
    fprintf(stderr, "Error: entry file not found: %s\n", entry);
    free(entry);
    return 1;
  }

  if (!quiet) printf("Compiling %s -> %s...\n", entry, backend->name);
  fflush(stdout);

  int scaffold_rc = 1;

  /* IR-first scaffold path */
  if (backend->scaffold_from_ir || backend->scaffold_from_ast) {
    CompileResult result = compiler_parse_project(entry);
    if (!result.ok || !result.ast) {
      fprintf(stderr, "Error: failed to parse%s%s\n",
              result.error ? ": " : "", result.error ? result.error : "");
      compiler_result_free(&result);
      free(entry);
      return 1;
    }
    IrProgram *ir = ir_from_ast(result.ast->root, entry);
    if (backend->scaffold_from_ir && ir) {
      scaffold_rc = backend->scaffold_from_ir(dir, ir);
    } else if (backend->scaffold_from_ast) {
      scaffold_rc = backend->scaffold_from_ast(dir, result.ast->root);
    } else {
      scaffold_rc = 1;
    }
    ir_free(ir);
    compiler_result_free(&result);
    free(entry);
  } else {
    char *code = compile_service_file(entry, backend->name);
    free(entry);
    if (!code) return 1;

    if (!backend->scaffold) {
      fprintf(stderr, "Error: backend '%s' has no scaffold\n", backend->name);
      free(code);
      return 1;
    }

    scaffold_rc = backend->scaffold(dir, code);
    free(code);
  }

  return scaffold_rc != 0 ? 1 : 0;
}

typedef struct {
  const BackendPort *backend;
  const char *dir;
} WatchCtx;

static int watch_rebuild_cb(void *userdata) {
  WatchCtx *ctx = (WatchCtx *)userdata;
  int rc = run_scaffold_once(ctx->backend, ctx->dir, 1);
  if (rc == 0) {
    printf("rebuilt dist/%s\n", ctx->backend->name);
    fflush(stdout);
  } else {
    fprintf(stderr, "watch: rebuild failed (previous output kept)\n");
    fflush(stderr);
  }
  return rc;
}

int run_service_run(const char *backend_name, const char *project_dir,
                    int check, int watch) {
  const char *dir = project_dir && *project_dir ? project_dir : ".";
  backend_register_all();

  const BackendPort *backend = backend_find(backend_name);
  if (!backend) {
    fprintf(stderr, "Error: unknown backend '%s'\n", backend_name);
    fprintf(stderr, "Available: react, svelte | preview: cordlang run\n");
    return 1;
  }

  char *cfg = fs_join(dir, "cordlang.json");
  if (!fs_exists(cfg)) {
    fprintf(stderr, "Error: not a Cordlang project (missing cordlang.json)\n");
    fprintf(stderr, "Run: cordlang init\n");
    free(cfg);
    return 1;
  }
  free(cfg);

  if (run_scaffold_once(backend, dir, 0) != 0) return 1;

  if (check) {
    /* Only Node-based scaffolds support vite check; only on first build. */
    if (strcmp(backend->name, "react") != 0 &&
        strcmp(backend->name, "svelte") != 0) {
      fprintf(stderr, "Error: --check is only supported for react and svelte\n");
      return 1;
    }
    if (run_vite_check(dir, backend->name) != 0) return 1;
  }

  if (watch) {
    if (strcmp(backend->name, "react") != 0 &&
        strcmp(backend->name, "svelte") != 0) {
      fprintf(stderr, "Error: --watch is only supported for react and svelte\n");
      return 1;
    }
    WatchCtx ctx = {.backend = backend, .dir = dir};
    return watch_service_run(dir, watch_rebuild_cb, &ctx);
  }

  return 0;
}
