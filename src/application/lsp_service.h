#ifndef CORDLANG_LSP_SERVICE_H
#define CORDLANG_LSP_SERVICE_H

/* Minimal stdio JSON-RPC Language Server (A1 / G8 light).
 * Supports: initialize, shutdown, exit, textDocument/didOpen|didChange|didSave,
 * publishDiagnostics (via check), documentSymbol, definition (via goto). */
int lsp_service_run(void);

#endif
