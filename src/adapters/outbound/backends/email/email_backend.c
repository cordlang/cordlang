#include "adapters/outbound/backends/email/email_backend.h"
#include "adapters/outbound/backends/static_html/static_html.h"
#include "application/ports/fs_port.h"
#include "domain/ir.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *email_generate_from_ir(IrProgram *ir) {
  static_html_warn_spa_hooks(ir, "email");
  return static_html_generate_from_ir(ir);
}

char *email_generate(Node *root) {
  if (!root) return strdup("<!DOCTYPE html><html><body></body></html>\n");
  IrProgram *ir = ir_from_ast(root, NULL);
  if (!ir) return strdup("<!DOCTYPE html><html><body></body></html>\n");
  char *out = email_generate_from_ir(ir);
  ir_free(ir);
  return out ? out : strdup("<!DOCTYPE html><html><body></body></html>\n");
}

static int write_path(const char *dir, const char *rel, const char *content) {
  char *path = fs_join(dir, rel);
  if (!path) return -1;
  int rc = fs_write_file(path, content);
  free(path);
  return rc;
}

int email_scaffold(const char *project_dir, const char *html_doc) {
  char *out = fs_join(project_dir, "dist/email");
  if (!out) return -1;
  if (fs_mkdir_p(out) != 0) {
    free(out);
    return -1;
  }
  int rc = write_path(out, "index.html",
                      html_doc ? html_doc
                               : "<!DOCTYPE html><html><body></body></html>\n");
  if (rc == 0)
    printf("Email HTML written to: dist/email/index.html\n");
  else
    fprintf(stderr, "Error: failed writing email HTML\n");
  free(out);
  return rc;
}

int email_scaffold_from_ir(const char *project_dir, IrProgram *ir) {
  char *html = email_generate_from_ir(ir);
  if (!html) return -1;
  int rc = email_scaffold(project_dir, html);
  free(html);
  return rc;
}

static const BackendPort email_port = {
    .name = "email",
    .extension = ".html",
    .needs_node_check = 0,
    .generate_from_ir = email_generate_from_ir,
    .scaffold_from_ir = email_scaffold_from_ir,
    .generate = email_generate,
    .scaffold = email_scaffold,
    .scaffold_from_ast = NULL,
};

const BackendPort *email_backend_port(void) { return &email_port; }
