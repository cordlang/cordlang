#include "html_backend_internal.h"



int html_scaffold_from_ir(const char *project_dir, IrProgram *ir) {
  char *html = html_generate_from_ir(ir);
  if (!html) return -1;
  int rc = html_scaffold(project_dir, html);
  free(html);
  return rc;
}

int htmlb_write_path(const char *dir, const char *rel, const char *content) {
  char *path = fs_join(dir, rel);
  if (!path) return -1;
  int rc = fs_write_file(path, content);
  free(path);
  return rc;
}

int html_scaffold(const char *project_dir, const char *html_doc) {
  char *out = fs_join(project_dir, "dist/preview");
  if (!out) return -1;
  if (fs_mkdir_p(out) != 0) {
    free(out);
    return -1;
  }
  int rc = htmlb_write_path(out, "index.html", html_doc ? html_doc : "<!DOCTYPE html><html><body>empty</body></html>\n");
  if (rc == 0) {
    printf("Preview written to: dist/preview/index.html\n");
  } else {
    fprintf(stderr, "Error: failed writing preview HTML\n");
  }
  free(out);
  return rc;
}

static const BackendPort html_port = {
    .name = "html",
    .extension = ".html",
    .needs_node_check = 0,
    .generate_from_ir = html_generate_from_ir,
    .scaffold_from_ir = html_scaffold_from_ir,
    .generate = html_generate,
    .scaffold = html_scaffold,
    .scaffold_from_ast = NULL,
};

const BackendPort *html_backend_port(void) { return &html_port; }
