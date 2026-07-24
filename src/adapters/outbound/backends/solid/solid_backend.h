#ifndef CORDLANG_SOLID_BACKEND_H
#define CORDLANG_SOLID_BACKEND_H

#include "domain/ast.h"
#include "domain/ir.h"
#include "application/ports/backend_port.h"

/* IR-first — pure IrNode walkers (IR-2). AST APIs lower through ir_from_ast. */
char *solid_generate_from_ir(IrProgram *ir);
int solid_scaffold_from_ir(const char *project_dir, IrProgram *ir);

/* AST surface (thin IR wrappers) */
char *solid_generate(Node *root);

typedef int (*SolidWriteFn)(const char *rel_from_src, const char *content,
                            void *userdata);
int solid_emit_modules(Node *root, SolidWriteFn write_fn, void *userdata);
int solid_emit_modules_from_ir(IrProgram *ir, SolidWriteFn write_fn,
                               void *userdata);

int solid_scaffold(const char *project_dir, const char *app_jsx);
int solid_scaffold_from_ast(const char *project_dir, Node *root);

const BackendPort *solid_backend_port(void);

#endif
