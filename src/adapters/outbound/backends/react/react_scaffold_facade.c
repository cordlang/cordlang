#include "react_scaffold_internal.h"

static const BackendPort react_port = {
    .name = "react",
    .extension = ".jsx",
    .needs_node_check = 1,
    .generate_from_ir = react_generate_from_ir,
    .scaffold_from_ir = react_scaffold_from_ir,
    .generate = react_generate,
    .scaffold = react_scaffold,
    .scaffold_from_ast = react_scaffold_from_ast,
};

int react_scaffold_from_ast(const char *project_dir, Node *root) {
  if (!root) return -1;
  IrProgram *ir = ir_from_ast(root, NULL);
  if (!ir) return -1;
  int rc = react_scaffold_from_ir(project_dir, ir);
  ir_free(ir);
  return rc;
}

int react_scaffold(const char *project_dir, const char *app_jsx) {
  char *out = fs_join(project_dir, "dist/react");
  if (!out) return -1;
  if (fs_mkdir_p(out) != 0) {
    free(out);
    return -1;
  }
  int rc = react_scaffold_write_vite_skeleton(project_dir, out);
  {
    char *pj = fs_join(out, "package.json");
    if (pj) {
      preset_merge_package_json(pj, project_dir, PRESET_BACKEND_REACT);
      free(pj);
    }
  }
  rc |= react_scaffold_write_path(out, "src/App.jsx",
                   app_jsx ? app_jsx
                           : "export default function App(){return null}\n");
  free(out);
  return rc;
}

int react_scaffold_from_ir(const char *project_dir, IrProgram *ir) {
  if (!ir || !ir->root) return -1;

  char *out = fs_join(project_dir, "dist/react");
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

  int rc = react_scaffold_write_vite_skeleton(project_dir, out);
  if (rc != 0) {
    fprintf(stderr, "Error: failed writing Vite skeleton\n");
    free(out);
    return rc;
  }
  {
    char *pj = fs_join(out, "package.json");
    if (pj) {
      preset_merge_package_json(pj, project_dir, PRESET_BACKEND_REACT);
      free(pj);
    }
  }
  preset_write_bridges(out, project_dir, PRESET_BACKEND_REACT);

  react_scaffold_scaffold_public_assets(project_dir, out);

  /* theme.css from IR hooks only (no AST origin) */
  {
    char *theme_css = theme_css_generate_from_ir(ir);
    if (theme_css) {
      rc |= react_scaffold_write_path(out, "src/theme.css", theme_css);
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

  rc = react_emit_modules_from_ir(ir, react_scaffold_scaffold_write_src, &ctx);
  if (rc != 0 || ctx.rc != 0) {
    fprintf(stderr, "Error: failed writing React modules\n");
    free(out);
    return -1;
  }

  react_scaffold_print_tree(&ctx);
  free(out);
  return 0;
}

const BackendPort *react_backend_port(void) { return &react_port; }
