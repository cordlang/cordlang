#ifndef CORDLANG_EMAIL_BACKEND_H
#define CORDLANG_EMAIL_BACKEND_H

#include "domain/ast.h"
#include "domain/ir.h"
#include "application/ports/backend_port.h"

char *email_generate_from_ir(IrProgram *ir);
int email_scaffold_from_ir(const char *project_dir, IrProgram *ir);

char *email_generate(Node *root);
int email_scaffold(const char *project_dir, const char *html_doc);

const BackendPort *email_backend_port(void);

#endif
