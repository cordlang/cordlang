#ifndef CORDLANG_STATIC_HTML_H
#define CORDLANG_STATIC_HTML_H

#include "domain/ir.h"

/*
 * Shared email/PDF-safe static HTML emit from IR.
 * Tables + inline styles; no reactive JS. Snapshot state/props initials into
 * interpolations where possible. SPA hooks (state/@click/route) are unsupported
 * for interactivity — callers may warn via static_html_scan_spa_hooks().
 */
char *static_html_generate_from_ir(IrProgram *ir);

/* Returns bitmask: 1=state, 2=event, 4=route found under ir->root. */
unsigned static_html_scan_spa_hooks(IrProgram *ir);

/* Emit a one-line stderr note if SPA hooks present (optional diagnostics). */
void static_html_warn_spa_hooks(IrProgram *ir, const char *backend_name);

#endif
