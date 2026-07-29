#ifndef CORDLANG_CHECK_SERVICE_H
#define CORDLANG_CHECK_SERVICE_H

#include "domain/ast.h"
#include "domain/diag.h"

/* Lightweight type/semantic checks after project parse.
 * Fills *out with diagnostics (caller owns via diag_list_free).
 * Returns 0 if no errors (warnings OK), 1 if errors. */
int check_service_run(const char *entry_path, DiagList *out);

/* Same checks on an in-memory buffer (LSP didOpen/didChange).
 * file_label is used in diagnostic paths (may be a real path or "<buffer>"). */
int check_service_run_source(const char *file_label, const char *source,
                             size_t source_len, DiagList *out);

/* Run semantic checks on an already-parsed AST root (e.g. ESM JIT).
 * Returns 0 if no errors (warnings OK), 1 if errors. */
int check_service_on_ast(Node *root, const char *file_label, DiagList *out);

/*
 * Same traps as check_service_on_ast for a SINGLE .cord module (ESM JIT):
 * skips project-wide route target resolution and treats `use` imports as
 * known component names. Returns 0 if no errors, 1 if errors.
 */
int check_service_on_module(Node *root, const char *file_label, DiagList *out);

#endif
