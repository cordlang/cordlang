#include "application/preview_internal.h"

/* ── smoke ──────────────────────────────────────────────── */

static int smoke_check(PreviewCtx *ctx, const char *path, int want_status,
                       const char *want_ctype_sub, const char *want_body_sub) {
  DevResponse out;
  memset(&out, 0, sizeof(out));
  if (preview_handler("GET", path, ctx, &out) != 0) {
    fprintf(stderr, "smoke FAIL: handler error for %s\n", path);
    free(out.body);
    return 1;
  }
  int fail = 0;
  if (out.status != want_status) {
    fprintf(stderr, "smoke FAIL: %s status %d (want %d)\n", path, out.status,
            want_status);
    fail = 1;
  }
  if (want_ctype_sub &&
      (!out.content_type || !strstr(out.content_type, want_ctype_sub))) {
    fprintf(stderr, "smoke FAIL: %s content-type '%s' (want contains %s)\n", path,
            out.content_type ? out.content_type : "(null)", want_ctype_sub);
    fail = 1;
  }
  if (want_body_sub && (!out.body || !strstr(out.body, want_body_sub))) {
    fprintf(stderr, "smoke FAIL: %s body missing '%s'\n", path, want_body_sub);
    fail = 1;
  }
  free(out.body);
  return fail;
}

int preview_service_smoke(const char *project_dir) {
  const char *dir = project_dir && *project_dir ? project_dir : ".";
  backend_register_all();

  PreviewCtx ctx;
  if (preview_ctx_init(&ctx, dir) != 0) return 1;

  int failed = 0;
  failed += smoke_check(&ctx, "/", 200, "text/html", "script");
  failed += smoke_check(&ctx, ctx.entry_url, 200, "javascript", "export");
  failed += smoke_check(&ctx, "/@cord/runtime.js", 200, "javascript", "mount");
  failed += smoke_check(&ctx, "/@cord/base.css", 200, "text/css", NULL);
  failed += smoke_check(&ctx, "/no-such-asset.xyz", 404, "text/plain", "404");
  failed += smoke_check(&ctx, "/spa-route-without-ext", 200, "text/html", "app");

  /* R5: structured error overlay modules (parse + check). */
  {
    char bad_parse[1200];
    char bad_check[1200];
    snprintf(bad_parse, sizeof(bad_parse), "%s/src/__smoke_parse_err.cord",
             ctx.root);
    snprintf(bad_check, sizeof(bad_check), "%s/src/__smoke_check_err.cord",
             ctx.root);
    FILE *fp = fopen(bad_parse, "wb");
    if (fp) {
      fputs("foreign\n", fp);
      fclose(fp);
      failed += smoke_check(&ctx, "/src/__smoke_parse_err.cord", 200,
                            "javascript", "showCompileError");
      failed += smoke_check(&ctx, "/src/__smoke_parse_err.cord", 200,
                            "javascript", "line\\\"");
      remove(bad_parse);
    } else {
      fprintf(stderr, "smoke FAIL: cannot write parse-error fixture\n");
      failed++;
    }
    fp = fopen(bad_check, "wb");
    if (fp) {
      fputs("def Bad\n  col className=\"flex\"\n    p \"nope\"\n", fp);
      fclose(fp);
      failed += smoke_check(&ctx, "/src/__smoke_check_err.cord", 200,
                            "javascript", "showCompileError");
      failed += smoke_check(&ctx, "/src/__smoke_check_err.cord", 200,
                            "javascript", "jsx-attr");
      remove(bad_check);
    } else {
      fprintf(stderr, "smoke FAIL: cannot write check-error fixture\n");
      failed++;
    }
  }

  /* Entry with routes should emit a non-empty routes array when present. */
  {
    DevResponse out;
    memset(&out, 0, sizeof(out));
    if (preview_handler("GET", ctx.entry_url, &ctx, &out) == 0 && out.body) {
      if (strstr(out.body, "export const routes") &&
          strstr(out.body, "routes = [\n];")) {
        fprintf(stderr, "smoke FAIL: entry routes array is empty\n");
        failed++;
      }
    }
    free(out.body);
  }

  preview_ctx_free(&ctx);
  if (failed) {
    fprintf(stderr, "preview smoke: %d check(s) failed\n", failed);
    return 1;
  }
  printf("preview smoke: ok\n");
  return 0;
}

