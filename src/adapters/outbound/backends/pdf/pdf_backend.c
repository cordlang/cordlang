#include "adapters/outbound/backends/pdf/pdf_backend.h"
#include "adapters/outbound/backends/static_html/static_html.h"
#include "application/ports/fs_port.h"
#include "domain/ir.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *pdf_generate_from_ir(IrProgram *ir) {
  static_html_warn_spa_hooks(ir, "pdf");
  return static_html_generate_from_ir(ir);
}

char *pdf_generate(Node *root) {
  if (!root) return strdup("<!DOCTYPE html><html><body></body></html>\n");
  IrProgram *ir = ir_from_ast(root, NULL);
  if (!ir) return strdup("<!DOCTYPE html><html><body></body></html>\n");
  char *out = pdf_generate_from_ir(ir);
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

static const char *PDF_README =
    "# Cordlang PDF scaffold\n"
    "\n"
    "This backend emits **static HTML** (same snapshot as `email`) under\n"
    "`dist/pdf/index.html`. Cordlang does **not** bundle a PDF engine.\n"
    "\n"
    "## Convert HTML → PDF (external tools)\n"
    "\n"
    "Pick one when available on your PATH:\n"
    "\n"
    "```bash\n"
    "# Playwright (Chromium)\n"
    "npx playwright pdf dist/pdf/index.html dist/pdf/out.pdf\n"
    "\n"
    "# WeasyPrint\n"
    "weasyprint dist/pdf/index.html dist/pdf/out.pdf\n"
    "\n"
    "# wkhtmltopdf\n"
    "wkhtmltopdf dist/pdf/index.html dist/pdf/out.pdf\n"
    "```\n"
    "\n"
    "`cordlang run pdf --check` is a soft check: it looks for `weasyprint` or\n"
    "`npx` and prints conversion hints; it does **not** fail if tools are absent.\n"
    "\n"
    "See `docs/PDF.md`.\n";

int pdf_scaffold(const char *project_dir, const char *html_doc) {
  char *out = fs_join(project_dir, "dist/pdf");
  if (!out) return -1;
  if (fs_mkdir_p(out) != 0) {
    free(out);
    return -1;
  }
  int rc = write_path(out, "index.html",
                      html_doc ? html_doc
                               : "<!DOCTYPE html><html><body></body></html>\n");
  rc |= write_path(out, "README.md", PDF_README);
  if (rc == 0)
    printf("PDF HTML written to: dist/pdf/index.html (+ README.md)\n");
  else
    fprintf(stderr, "Error: failed writing PDF scaffold\n");
  free(out);
  return rc;
}

int pdf_scaffold_from_ir(const char *project_dir, IrProgram *ir) {
  char *html = pdf_generate_from_ir(ir);
  if (!html) return -1;
  int rc = pdf_scaffold(project_dir, html);
  free(html);
  return rc;
}

static const BackendPort pdf_port = {
    .name = "pdf",
    .extension = ".html",
    .needs_node_check = 0,
    .generate_from_ir = pdf_generate_from_ir,
    .scaffold_from_ir = pdf_scaffold_from_ir,
    .generate = pdf_generate,
    .scaffold = pdf_scaffold,
    .scaffold_from_ast = NULL,
};

const BackendPort *pdf_backend_port(void) { return &pdf_port; }
