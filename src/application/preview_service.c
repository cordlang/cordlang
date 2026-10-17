#include "application/preview_internal.h"

int preview_service_run(const char *project_dir, int open_browser) {
  const char *dir = project_dir && *project_dir ? project_dir : ".";
  backend_register_all();

  char *cfg = fs_join(dir, "cordlang.json");
  if (!cfg || !fs_exists(cfg)) {
    fprintf(stderr, "Error: not a Cordlang project (missing cordlang.json)\n");
    fprintf(stderr, "Run: cordlang init\n");
    free(cfg);
    return 1;
  }
  free(cfg);

  PreviewCtx *ctx = calloc(1, sizeof(PreviewCtx));
  if (!ctx) return 1;
  if (preview_ctx_init(ctx, dir) != 0) {
    free(ctx);
    return 1;
  }

  {
    CompileResult probe = compiler_parse_project(ctx->entry_abs);
    if (!probe.ok || !probe.ast) {
      fprintf(stderr, "Error: %s\n",
              probe.error ? probe.error : "el proyecto no compila");
      compiler_result_free(&probe);
      preview_ctx_free(ctx);
      free(ctx);
      return 1;
    }
    compiler_result_free(&probe);
  }

  term_info("serving %s", ctx->entry_url);
  fflush(stdout);

  int rc = dev_server_serve(4173, dir, ctx->entry_url, open_browser,
                            preview_handler, ctx, on_watch_change);
  preview_ctx_free(ctx);
  free(ctx);
  return rc;
}

int preview_service_run_html(const char *project_dir) {
  const char *dir = project_dir && *project_dir ? project_dir : ".";
  backend_register_all();

  const BackendPort *backend = backend_find("html");
  if (!backend) {
    fprintf(stderr, "Error: HTML runtime backend not registered\n");
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

  char *entry_rel = read_entry_from_config(dir);
  char *entry = fs_join(dir, entry_rel ? entry_rel : "src/app.cord");
  free(entry_rel);

  if (!entry || !fs_exists(entry)) {
    fprintf(stderr, "Error: entry file not found: %s\n", entry ? entry : "?");
    free(entry);
    return 1;
  }

  printf("Compiling %s -> native HTML runtime...\n", entry);
  char *html = compile_service_file(entry, "html");
  free(entry);
  if (!html) return 1;

  if (backend->scaffold) backend->scaffold(dir, html);

  size_t html_len = strlen(html);
  int rc = preview_server_serve(html, html_len, 4173);
  free(html);
  return rc;
}
