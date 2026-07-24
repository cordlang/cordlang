#ifndef CORDLANG_VUE_BACKEND_H
#define CORDLANG_VUE_BACKEND_H

#include "domain/ast.h"
#include "domain/ir.h"
#include "application/ports/backend_port.h"

/* IR-first (preferred) */
char *vue_generate_from_ir(IrProgram *ir);
int vue_scaffold_from_ir(const char *project_dir, IrProgram *ir);

/* Legacy AST surface (wraps IR) */
char *vue_generate(Node *root);

typedef int (*VueWriteFn)(const char *rel_from_src, const char *content,
                             void *userdata);
/* Pure IR module emit (IR-2). */
int vue_emit_modules_from_ir(IrProgram *ir, VueWriteFn write_fn,
                                void *userdata);
/* Legacy: AST → IR → emit. */
int vue_emit_modules(Node *root, VueWriteFn write_fn, void *userdata);

int vue_scaffold(const char *project_dir, const char *blob);
int vue_scaffold_from_ast(const char *project_dir, Node *root);

const BackendPort *vue_backend_port(void);

#endif
