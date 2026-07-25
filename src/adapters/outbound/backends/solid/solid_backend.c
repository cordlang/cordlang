#include "adapters/outbound/backends/solid/solid_backend.h"
#include "domain/ir.h"
#include <stdlib.h>
#include <string.h>

/* AST surface is a thin IR lower — all codegen lives in solid_ir.c. */

int solid_emit_modules(Node *root, SolidWriteFn write_fn, void *userdata) {
  if (!root) return -1;
  IrProgram *ir = ir_from_ast(root, NULL);
  if (!ir) return -1;
  int rc = solid_emit_modules_from_ir(ir, write_fn, userdata);
  ir_free(ir);
  return rc;
}

char *solid_generate(Node *root) {
  if (!root) return strdup("export default function App(){return null}\n");
  IrProgram *ir = ir_from_ast(root, NULL);
  if (!ir) return strdup("export default function App(){return null}\n");
  char *out = solid_generate_from_ir(ir);
  ir_free(ir);
  if (!out) return strdup("export default function App(){return null}\n");
  return out;
}
