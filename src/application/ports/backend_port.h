#ifndef CORDLANG_BACKEND_PORT_H
#define CORDLANG_BACKEND_PORT_H

#include "domain/ast.h"
#include "domain/ir.h"

/*
 * Outbound port: compile Cordlang to a target framework.
 *
 * Primary path (IR-first, post Phase C follow-up):
 *   AST → ir_from_ast → generate_from_ir / scaffold_from_ir
 *
 * Legacy AST entrypoints remain for tests and gradual migration; they
 * must go through IR when possible (wrappers in each backend).
 */
typedef struct BackendPort {
  const char *name;
  const char *extension;
  int needs_node_check; /* 1 = vite --check/--watch supported */

  /* Preferred: codegen from canonical IR (AST must still be alive for origins). */
  char *(*generate_from_ir)(IrProgram *ir);
  int (*scaffold_from_ir)(const char *project_dir, IrProgram *ir);

  /* Legacy AST surface (implemented as IR wrappers where possible). */
  char *(*generate)(Node *root);
  int (*scaffold)(const char *project_dir, const char *generated);
  int (*scaffold_from_ast)(const char *project_dir, Node *root);
} BackendPort;

const BackendPort *backend_find(const char *name);
/* Fill names[0..*out_n) with registered backend names; returns count. */
int backend_list(const char **names, int max);
void backend_register_all(void);

#endif
