#include "adapters/outbound/backends/react/react_backend.h"
#include "domain/ir.h"
#include <stdlib.h>
#include <string.h>

/* AST surface is a thin IR lower — all codegen lives in react_ir.c. */

int react_emit_modules(Node *root, ReactWriteFn write_fn, void *userdata) {
  if (!root) return -1;
  IrProgram *ir = ir_from_ast(root, NULL);
  if (!ir) return -1;
  int rc = react_emit_modules_from_ir(ir, write_fn, userdata);
  ir_free(ir);
  return rc;
}

char *react_generate(Node *root) {
  if (!root) return strdup("export default function App(){return null}\n");
  IrProgram *ir = ir_from_ast(root, NULL);
  if (!ir) return strdup("export default function App(){return null}\n");
  char *out = react_generate_from_ir(ir);
  ir_free(ir);
  if (!out) return strdup("export default function App(){return null}\n");
  return out;
}
