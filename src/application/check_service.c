#include "application/check_internal.h"

void check_routes(const CheckCtx *ctx, const NameSet *defs, DiagList *out,
                         const char *file) {
  for (int i = 0; i < ctx->n_routes; i++) {
    const RouteInfo *r = &ctx->routes[i];
    if (!r->target[0]) {
      diag_emit(out, DIAG_ERROR, file, r->line, r->col,
                "route target missing for path '%s'",
                r->path[0] ? r->path : "?");
      continue;
    }
    if (!name_set_has(defs, r->target)) {
      diag_emit(out, DIAG_ERROR, file, r->line, r->col,
                "route target '%s' is not a known component", r->target);
    }
  }
}

int check_service_on_ast(Node *root, const char *file_label, DiagList *out) {
  if (!out || !root) return 1;
  NameSet defs = {0};
  NameSet dups = {0};
  CheckCtx ctx = {0};

  load_presets_for_entry(file_label, &ctx);
  collect_defs(root, &defs, &dups, out, file_label);
  collect_contexts_and_routes(root, &ctx);
  check_routes(&ctx, &defs, out, file_label);
  check_prop_types(root, out, file_label);
  walk_checks(root, &defs, &ctx, NULL, out, file_label);
  return diag_error_count(out) > 0 ? 1 : 0;
}

void collect_use_names(Node *n, NameSet *defs) {
  if (!n || !defs) return;
  if (n->type == NODE_USE && n->value && n->value[0]) {
    char *en = compiler_module_export_name(n->value, n->value2);
    if (en && en[0]) name_set_add(defs, en, n->line, n->col);
    free(en);
  }
  for (size_t i = 0; i < n->children_len; i++)
    collect_use_names(n->children[i], defs);
}

int check_service_on_module(Node *root, const char *file_label, DiagList *out) {
  if (!out || !root) return 1;
  NameSet defs = {0};
  NameSet dups = {0};
  CheckCtx ctx = {0};

  load_presets_for_entry(file_label, &ctx);
  collect_defs(root, &defs, &dups, out, file_label);
  collect_use_names(root, &defs);
  collect_contexts_and_routes(root, &ctx);
  /* Skip check_routes: route targets live in other modules in the ESM graph. */
  check_prop_types(root, out, file_label);
  walk_checks(root, &defs, &ctx, NULL, out, file_label);
  return diag_error_count(out) > 0 ? 1 : 0;
}

int check_service_run_source(const char *file_label, const char *source,
                             size_t source_len, DiagList *out) {
  if (!out) return 1;
  const char *label = (file_label && *file_label) ? file_label : "<buffer>";
  if (!source) {
    diag_emit(out, DIAG_ERROR, label, 0, 0, "no source");
    return 1;
  }

  CompileResult result = compiler_parse_source(source, source_len);
  if (!result.ok || !result.ast || !result.ast->root) {
    diag_emit(out, DIAG_ERROR, label,
              result.error_line > 0 ? result.error_line : 0,
              result.error_col > 0 ? result.error_col : 0, "%s",
              result.error ? result.error : "parse failed");
    compiler_result_free(&result);
    return 1;
  }

  int rc = check_service_on_ast(result.ast->root, label, out);
  compiler_result_free(&result);
  return rc;
}

int check_service_run(const char *entry_path, DiagList *out) {
  if (!out) return 1;
  /* out may already be initialized by caller; do not wipe existing diags */

  if (!entry_path || !*entry_path) {
    diag_emit(out, DIAG_ERROR, "<check>", 0, 0, "no entry path");
    return 1;
  }

  CompileResult result = compiler_parse_project(entry_path);
  if (!result.ok || !result.ast || !result.ast->root) {
    diag_emit(out, DIAG_ERROR, entry_path,
              result.error_line > 0 ? result.error_line : 0,
              result.error_col > 0 ? result.error_col : 0, "%s",
              result.error ? result.error : "parse failed");
    compiler_result_free(&result);
    return 1;
  }

  int rc = check_service_on_ast(result.ast->root, entry_path, out);
  compiler_result_free(&result);
  return rc;
}
