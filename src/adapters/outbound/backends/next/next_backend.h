#ifndef CORDLANG_NEXT_BACKEND_H
#define CORDLANG_NEXT_BACKEND_H

#include "domain/ast.h"
#include "domain/ir.h"
#include "application/ports/backend_port.h"

char *next_generate_from_ir(IrProgram *ir);
int next_scaffold_from_ir(const char *project_dir, IrProgram *ir);

char *next_generate(Node *root);
int next_scaffold_from_ast(const char *project_dir, Node *root);

const BackendPort *next_backend_port(void);

#endif
