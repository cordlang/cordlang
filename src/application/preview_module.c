#include "application/preview_internal.h"

 int path_has_dotdot(const char *p) {
  return p && (strstr(p, "..") != NULL);
}

 int same_file(const char *a, const char *b) {
  char *na = fs_norm_path(a);
  char *nb = fs_norm_path(b);
  int eq = 0;
  if (na && nb) {
#ifdef _WIN32
    size_t i = 0;
    eq = 1;
    for (; na[i] && nb[i]; i++) {
      char ca = na[i], cb = nb[i];
      if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
      if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
      if (ca != cb) {
        eq = 0;
        break;
      }
    }
    if (eq && (na[i] || nb[i])) eq = 0;
#else
    eq = strcmp(na, nb) == 0;
#endif
  }
  free(na);
  free(nb);
  return eq;
}

 char *bust_query(PreviewCtx *ctx, char *buf, size_t n) {
  snprintf(buf, n, "?v=%llu", (unsigned long long)ctx->bust);
  return buf;
}

 int respond_cord_module(PreviewCtx *ctx, const char *path,
                               DevResponse *out) {
  if (path_has_dotdot(path)) return respond_not_found(out, path);

  const char *rel = path;
  while (*rel == '/') rel++;

  char *abs = fs_join(ctx->root, rel);
  if (!abs) return -1;
  if (!fs_exists(abs) || fs_is_dir(abs)) {
    free(abs);
    return respond_not_found(out, path);
  }

  long long mtime = 0, size = 0;
  file_mtime_size(abs, &mtime, &size);

  PreviewCacheEntry *hit = cache_find(ctx, path);
  if (hit && hit->mtime == mtime && hit->size == size && hit->bust == ctx->bust) {
    free(abs);
    return respond_cached_copy(out, hit);
  }

  CompileResult r = compiler_parse_file(abs);
  if (!r.ok || !r.ast) {
    DiagList diags;
    diag_list_init(&diags);
    diag_emit(&diags, DIAG_ERROR, rel,
              r.error_line > 0 ? r.error_line : 0,
              r.error_col > 0 ? r.error_col : 0, "%s",
              r.error ? r.error : "error de parseo");
    preview_log_compile_error(ctx, rel,
                              r.error_line > 0 ? r.error_line : 0,
                              r.error_col > 0 ? r.error_col : 0,
                              r.error ? r.error : "error de parseo");
    size_t src_len = 0;
    char *src = fs_read_file(abs, &src_len);
    compiler_result_free(&r);
    free(abs);
    char *mod = esm_error_module_from_diags(&diags, src, src_len);
    free(src);
    diag_list_free(&diags);
    if (!mod) return -1;
    return respond_str(out, 200, "text/javascript; charset=utf-8", mod,
                       strlen(mod), 1);
  }

  int is_entry = same_file(abs, ctx->entry_abs);

  char export_name[128];
  export_name[0] = '\0';
  if (!is_entry) {
    char *en = compiler_module_export_name(rel, NULL);
    if (en) {
      snprintf(export_name, sizeof(export_name), "%s", en);
      free(en);
    }
    compiler_wrap_module_body(r.ast->root, export_name, abs);
  }

  {
    DiagList diags;
    diag_list_init(&diags);
    if (check_service_on_module(r.ast->root, rel, &diags) != 0) {
      const Diagnostic *d0 = diags.len ? &diags.items[0] : NULL;
      preview_log_compile_error(
          ctx, rel, d0 ? d0->line : 0, d0 ? d0->col : 0,
          d0 && d0->message ? d0->message : "check failed");
      size_t src_len = 0;
      char *src = r.ast->source ? strdup(r.ast->source) : fs_read_file(abs, &src_len);
      if (r.ast->source && src) src_len = strlen(src);
      compiler_result_free(&r);
      free(abs);
      char *mod = esm_error_module_from_diags(&diags, src, src_len);
      free(src);
      diag_list_free(&diags);
      if (!mod) return -1;
      return respond_str(out, 200, "text/javascript; charset=utf-8", mod,
                         strlen(mod), 1);
    }
    diag_list_free(&diags);
  }

  IrProgram *ir = ir_from_ast(r.ast->root, abs);
  if (!ir) {
    compiler_result_free(&r);
    free(abs);
    return -1;
  }

  EsmModuleCtx mod;
  memset(&mod, 0, sizeof(mod));
  mod.kind = is_entry ? ESM_MOD_ENTRY : ESM_MOD_COMPONENT;
  mod.export_name = export_name[0] ? export_name : NULL;
  mod.source_rel = rel;
  mod.abs_path = abs;
  mod.project_root = ctx->root_abs[0] ? ctx->root_abs : ctx->root;
  /* Bust imports so soft remount re-fetches the graph without full reload. */
  {
    char qbuf[64];
    mod.import_query = bust_query(ctx, qbuf, sizeof(qbuf));
    mod.truncated = 0;

    char *js = esm_generate_module(ir, &mod);
    ir_free(ir);
    compiler_result_free(&r);

    if (mod.truncated)
      fprintf(stderr,
              "cordlang preview: import/foreign limit reached in %s (some "
              "modules omitted)\n",
              rel);

    free(abs);
    if (!js) return -1;

    size_t len = strlen(js);
    cache_put(ctx, path, mtime, size, "text/javascript; charset=utf-8",
              strdup(js), len);
    free(js);
    hit = cache_find(ctx, path);
    if (hit) return respond_cached_copy(out, hit);
    return -1;
  }
}

