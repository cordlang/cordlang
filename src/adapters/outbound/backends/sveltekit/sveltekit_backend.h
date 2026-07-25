#ifndef CORDLANG_SVELTEKIT_BACKEND_H
#define CORDLANG_SVELTEKIT_BACKEND_H

#include "domain/ast.h"
#include "domain/ir.h"
#include "application/ports/backend_port.h"

char *sveltekit_generate_from_ir(IrProgram *ir);
int sveltekit_scaffold_from_ir(const char *project_dir, IrProgram *ir);

char *sveltekit_generate(Node *root);
int sveltekit_scaffold_from_ast(const char *project_dir, Node *root);

const BackendPort *sveltekit_backend_port(void);

#endif
