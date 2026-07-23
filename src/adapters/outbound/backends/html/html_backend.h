#ifndef CORDLANG_HTML_BACKEND_H
#define CORDLANG_HTML_BACKEND_H

#include "domain/ast.h"
#include "domain/ir.h"
#include "application/ports/backend_port.h"

/* IR-first (preferred) */
char *html_generate_from_ir(IrProgram *ir);
int html_scaffold_from_ir(const char *project_dir, IrProgram *ir);

/* Legacy AST surface */
char *html_generate(Node *root);

/* Write preview scaffold to dist/preview/index.html */
int html_scaffold(const char *project_dir, const char *html_doc);

const BackendPort *html_backend_port(void);

#endif
