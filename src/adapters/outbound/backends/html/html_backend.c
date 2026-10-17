#include "html_backend_internal.h"

char *html_generate(Node *root) {
  if (!root)
    return strdup("<!DOCTYPE html><html><body>empty</body></html>\n");
  IrProgram *ir = ir_from_ast(root, NULL);
  if (!ir) return htmlb_html_generate_impl(root);
  char *out = html_generate_from_ir(ir);
  ir_free(ir);
  return out;
}

char *html_generate_from_ir(IrProgram *ir) {
  /* Walk ir->root only — never ir_project_origin / AST for body or state. */
  if (!ir || !ir->root)
    return strdup("<!DOCTYPE html><html><body>empty</body></html>\n");
  return htmlb_html_generate_from_ir_root(ir->root);
}
