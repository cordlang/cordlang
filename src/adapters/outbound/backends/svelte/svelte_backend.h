#ifndef CORDLANG_SVELTE_BACKEND_H
#define CORDLANG_SVELTE_BACKEND_H

#include "domain/ast.h"
#include "domain/ir.h"
#include "application/ports/backend_port.h"

/* IR-first (preferred) */
char *svelte_generate_from_ir(IrProgram *ir);
int svelte_scaffold_from_ir(const char *project_dir, IrProgram *ir);

/* Legacy AST surface (wraps IR) */
char *svelte_generate(Node *root);

typedef int (*SvelteWriteFn)(const char *rel_from_src, const char *content,
                             void *userdata);
/* Pure IR module emit (IR-2). */
int svelte_emit_modules_from_ir(IrProgram *ir, SvelteWriteFn write_fn,
                                void *userdata);
/* Legacy: AST → IR → emit. */
int svelte_emit_modules(Node *root, SvelteWriteFn write_fn, void *userdata);

int svelte_scaffold(const char *project_dir, const char *blob);
int svelte_scaffold_from_ast(const char *project_dir, Node *root);

const BackendPort *svelte_backend_port(void);

#endif
