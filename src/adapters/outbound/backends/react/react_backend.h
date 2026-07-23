#ifndef CORDLANG_REACT_BACKEND_H
#define CORDLANG_REACT_BACKEND_H

#include "domain/ast.h"
#include "domain/ir.h"
#include "application/ports/backend_port.h"

/* IR-first (preferred) — pure IrNode walkers (IR-2) */
char *react_generate_from_ir(IrProgram *ir);
int react_scaffold_from_ir(const char *project_dir, IrProgram *ir);

/* Legacy AST surface (wraps IR) */
char *react_generate(Node *root);

typedef int (*ReactWriteFn)(const char *rel_from_src, const char *content,
                            void *userdata);
int react_emit_modules(Node *root, ReactWriteFn write_fn, void *userdata);
/* IR-2 multi-file emit (no origin AST) */
int react_emit_modules_from_ir(IrProgram *ir, ReactWriteFn write_fn,
                               void *userdata);

int react_scaffold(const char *project_dir, const char *app_jsx);
int react_scaffold_from_ast(const char *project_dir, Node *root);

const BackendPort *react_backend_port(void);

#endif
