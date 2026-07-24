#ifndef CORDLANG_PDF_BACKEND_H
#define CORDLANG_PDF_BACKEND_H

#include "domain/ast.h"
#include "domain/ir.h"
#include "application/ports/backend_port.h"

char *pdf_generate_from_ir(IrProgram *ir);
int pdf_scaffold_from_ir(const char *project_dir, IrProgram *ir);

char *pdf_generate(Node *root);
int pdf_scaffold(const char *project_dir, const char *html_doc);

const BackendPort *pdf_backend_port(void);

#endif
