#include "svelte_scaffold_internal.h"

static const BackendPort svelte_port = {
    .name = "svelte",
    .extension = ".svelte",
    .needs_node_check = 1,
    .generate_from_ir = svelte_generate_from_ir,
    .scaffold_from_ir = svelte_scaffold_from_ir,
    .generate = svelte_generate,
    .scaffold = svelte_scaffold,
    .scaffold_from_ast = svelte_scaffold_from_ast,
};

int svelte_scaffold_from_ast(const char *project_dir, Node *root) {
  char *out = fs_join(project_dir, "dist/svelte");
  if (!out) return -1;
  if (fs_mkdir_p(out) != 0) {
    free(out);
    return -1;
  }

  char *p1 = fs_join(out, "src/pages");
  char *p2 = fs_join(out, "src/components");
  char *p3 = fs_join(out, "src/layouts");
  if (p1) {
    fs_mkdir_p(p1);
    free(p1);
  }
  if (p2) {
    fs_mkdir_p(p2);
    free(p2);
  }
  if (p3) {
    fs_mkdir_p(p3);
    free(p3);
  }

  int rc = svelte_scaffold_write_vite_skeleton(project_dir, out);
  if (rc != 0) {
    fprintf(stderr, "Error: failed writing Svelte skeleton\n");
    free(out);
    return rc;
  }
  {
    char *pj = fs_join(out, "package.json");
    if (pj) {
      preset_merge_package_json(pj, project_dir, PRESET_BACKEND_SVELTE);
      free(pj);
    }
  }
  preset_write_bridges(out, project_dir, PRESET_BACKEND_SVELTE);

  svelte_scaffold_scaffold_public_assets(project_dir, out);

  {
    char *theme_css = theme_css_generate(root);
    if (theme_css) {
      rc |= svelte_scaffold_write_path(out, "src/theme.css", theme_css);
      free(theme_css);
    }
    if (rc != 0) {
      fprintf(stderr, "Error: failed writing theme.css\n");
      free(out);
      return rc;
    }
    char *fonts_dir = fs_join(out, "public/fonts");
    if (fonts_dir) {
      font_cache_install_from_ast(root, fonts_dir);
      free(fonts_dir);
    }
  }

  ScaffoldCtx ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.out_root = out;

  rc = svelte_emit_modules(root, svelte_scaffold_scaffold_write_src, &ctx);
  if (rc != 0 || ctx.rc != 0) {
    fprintf(stderr, "Error: failed writing Svelte modules\n");
    free(out);
    return -1;
  }

  svelte_scaffold_print_scaffold_summary(&ctx);

  free(out);
  return 0;
}

int svelte_scaffold(const char *project_dir, const char *blob) {
  char *out = fs_join(project_dir, "dist/svelte");
  if (!out) return -1;
  if (fs_mkdir_p(out) != 0) {
    free(out);
    return -1;
  }
  int rc = svelte_scaffold_write_vite_skeleton(project_dir, out);
  rc |= svelte_scaffold_write_path(out, "src/App.svelte",
                   blob ? blob : "<script></script>\n<p>Empty</p>\n");
  free(out);
  return rc;
}

int svelte_scaffold_from_ir(const char *project_dir, IrProgram *ir) {
  if (!ir || !ir->root) return -1;

  char *out = fs_join(project_dir, "dist/svelte");
  if (!out) return -1;
  if (fs_mkdir_p(out) != 0) {
    free(out);
    return -1;
  }

  char *p1 = fs_join(out, "src/pages");
  char *p2 = fs_join(out, "src/components");
  char *p3 = fs_join(out, "src/layouts");
  if (p1) {
    fs_mkdir_p(p1);
    free(p1);
  }
  if (p2) {
    fs_mkdir_p(p2);
    free(p2);
  }
  if (p3) {
    fs_mkdir_p(p3);
    free(p3);
  }

  int rc = svelte_scaffold_write_vite_skeleton(project_dir, out);
  if (rc != 0) {
    fprintf(stderr, "Error: failed writing Svelte skeleton\n");
    free(out);
    return rc;
  }
  {
    char *pj = fs_join(out, "package.json");
    if (pj) {
      preset_merge_package_json(pj, project_dir, PRESET_BACKEND_SVELTE);
      free(pj);
    }
  }
  preset_write_bridges(out, project_dir, PRESET_BACKEND_SVELTE);

  svelte_scaffold_scaffold_public_assets(project_dir, out);

  /* theme.css from IR hooks only (no AST origin) */
  {
    char *theme_css = theme_css_generate_from_ir(ir);
    if (theme_css) {
      rc |= svelte_scaffold_write_path(out, "src/theme.css", theme_css);
      free(theme_css);
    }
    if (rc != 0) {
      fprintf(stderr, "Error: failed writing theme.css\n");
      free(out);
      return rc;
    }
    char *fonts_dir = fs_join(out, "public/fonts");
    if (fonts_dir) {
      font_cache_install_from_ir(ir, fonts_dir);
      free(fonts_dir);
    }
  }

  ScaffoldCtx ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.out_root = out;

  /* Pure IR body emission (IR-2) */
  rc = svelte_emit_modules_from_ir(ir, svelte_scaffold_scaffold_write_src, &ctx);
  if (rc != 0 || ctx.rc != 0) {
    fprintf(stderr, "Error: failed writing Svelte modules\n");
    free(out);
    return -1;
  }

  svelte_scaffold_print_scaffold_summary(&ctx);
  free(out);
  return 0;
}

const BackendPort *svelte_backend_port(void) { return &svelte_port; }
