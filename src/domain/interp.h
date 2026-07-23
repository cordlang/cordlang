#ifndef CORDLANG_INTERP_H
#define CORDLANG_INTERP_H

#include "domain/ast.h"

/* True if s contains at least one #{...} interpolation. */
int interp_has(const char *s);

/*
 * Split a Cordlang string with optional #{expr} parts into AST children of
 * parent:
 *   - NODE_TEXT          for literal segments
 *   - NODE_INTERPOLATION for each #{expr} (value = expression)
 * If there is no interpolation, adds a single NODE_TEXT.
 */
void interp_add_to_node(Node *parent, const char *str, int line, int col);

/*
 * Build one node for a string:
 *   - plain → NODE_TEXT
 *   - with #{} → NODE_INTERPOLATION container (value=NULL) with TEXT/INTERP children
 */
Node *interp_make_node(const char *str, int line, int col);

/*
 * Convert "Hello #{user.name}!" → Hello ${user.name}!
 * Suitable for JS template literal body (inside `...`).
 * Caller frees. Escapes `, \, and bare $ that is not part of ${.
 */
char *interp_to_js_template_body(const char *str);

/*
 * HTML preview: literal text + <span class="cl-interp">${expr}</span>
 * Caller frees.
 */
char *interp_to_html_fragment(const char *str);

#endif
