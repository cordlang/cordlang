#include "application/run_service.h"
#include "application/compile_service.h"
#include "application/watch_service.h"
#include "application/ports/backend_port.h"
#include "application/ports/compiler_port.h"
#include "application/ports/fs_port.h"
#include "adapters/outbound/process/process_spawn.h"
#include "adapters/outbound/json/json_mini.h"
#include "domain/ir.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define NPM_CMD "npm.cmd"
#else
#define NPM_CMD "npm"
#endif

static char *read_entry_from_config(const char *project_dir) {
  char *cfg = fs_join(project_dir, "cordlang.json");
  if (!cfg) return NULL;

  size_t len = 0;
  char *json = fs_read_file(cfg, &len);
  free(cfg);
  if (!json) return NULL;

  char *entry = json_object_get_string(json, "entry");
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

  int rc = 0;

  if (need_install) {
    printf("--check: node_modules missing, running npm install...\n");
    fflush(stdout);
    /*
     * Run npm *inside* dist/, not via --prefix from the project root.
     * On Windows, `npm.cmd install --prefix <path>` often ignores prefix and
     * looks for package.json in the current directory (ENOENT on templates/).
     * process_run(cwd=dist) matches how we already run `npm run build`.
     */
    char *argv_install[] = {NPM_CMD, "install", NULL};
    rc = process_run(dist, argv_install, 1);
    if (rc != 0) {
      fprintf(stderr, "Error: --check: npm install failed (exit %d)\n", rc);
      free(dist);
      return 1;
    }
  } else {
    printf("--check: node_modules present, skipping npm install\n");
  }

  /* Prefer package.json "build" (Vite / Next / SvelteKit). Fallback: vite. */
  printf("--check: running npm run build...\n");
  fflush(stdout);
  char *argv_build[] = {NPM_CMD, "run", "build", NULL};
  rc = process_run(dist, argv_build, 1);
  if (rc != 0 && strcmp(backend_name, "next") != 0 &&
      strcmp(backend_name, "sveltekit") != 0) {
    printf("--check: npm run build failed; trying vite build...\n");
    fflush(stdout);
    char *argv_vite[] = {NPM_CMD, "exec", "--", "vite", "build", NULL};
    rc = process_run(dist, argv_vite, 1);
  }

  free(dist);
  if (rc != 0) {
    fprintf(stderr, "Error: --check: build failed (exit %d)\n", rc);
    return 1;
  }
  printf("--check: build OK\n");
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
    {
      const char *names[16];
      int n = backend_list(names, 16);
      fprintf(stderr, "Available:");
      int first = 1;
      for (int i = 0; i < n; i++) {
        if (!names[i] || strcmp(names[i], "html") == 0) continue;
        fprintf(stderr, "%s%s", first ? " " : ", ", names[i]);
        first = 0;
      }
      fprintf(stderr, " | preview: cordlang run\n");
    }
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
    /* PDF: soft external-tool hint (no hard failure if converter missing). */
    if (strcmp(backend->name, "pdf") == 0) {
      printf("\n--check (pdf): looking for HTML→PDF converters...\n");
#ifdef _WIN32
      printf("--check (pdf): convert dist/pdf/index.html externally "
             "(weasyprint / playwright / wkhtmltopdf). See docs/PDF.md\n");
#else
      {
        char *which_w[] = {"which", "weasyprint", NULL};
        char *which_n[] = {"which", "npx", NULL};
        int has_weasy = process_run(NULL, which_w, 1) == 0;
        int has_npx = process_run(NULL, which_n, 1) == 0;
        if (has_weasy) {
          printf("--check (pdf): weasyprint found. Example:\n");
          printf("  weasyprint dist/pdf/index.html dist/pdf/out.pdf\n");
        } else if (has_npx) {
          printf("--check (pdf): npx found. Example:\n");
          printf("  npx playwright pdf dist/pdf/index.html dist/pdf/out.pdf\n");
        } else {
          printf("--check (pdf): no converter on PATH — skip. "
                 "See dist/pdf/README.md and docs/PDF.md\n");
        }
      }
#endif
    } else if (!backend->needs_node_check) {
      fprintf(stderr, "Error: --check is not supported for backend '%s'\n",
              backend->name);
      return 1;
    } else if (run_vite_check(dir, backend->name) != 0) {
      return 1;
    }
  }

  if (watch) {
    if (!backend->needs_node_check) {
      fprintf(stderr, "Error: --watch is not supported for backend '%s'\n",
              backend->name);
      return 1;
    }
    WatchCtx ctx = {.backend = backend, .dir = dir};
    return watch_service_run(dir, watch_rebuild_cb, &ctx);
  }

  return 0;
}
