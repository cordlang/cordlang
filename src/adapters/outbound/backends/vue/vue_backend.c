#include "adapters/outbound/backends/vue/vue_backend.h"
#include "domain/ir.h"
#include <stdlib.h>
#include <string.h>

/* AST surface is a thin IR lower — all codegen lives in vue_ir.c. */

int vue_emit_modules(Node *root, VueWriteFn write_fn, void *userdata) {
  if (!root) return -1;
  IrProgram *ir = ir_from_ast(root, NULL);
  if (!ir) return -1;
  int rc = vue_emit_modules_from_ir(ir, write_fn, userdata);
  ir_free(ir);
  return rc;
}

char *vue_generate(Node *root) {
  if (!root) return strdup("<!-- empty -->\n");
  IrProgram *ir = ir_from_ast(root, NULL);
  if (!ir) return strdup("<!-- empty -->\n");
  char *out = vue_generate_from_ir(ir);
  ir_free(ir);
  if (!out) return strdup("<!-- empty -->\n");
  return out;
}
