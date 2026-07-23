#ifndef CORDLANG_THEME_CSS_H
#define CORDLANG_THEME_CSS_H

#include "domain/ast.h"
#include "domain/ir.h"

/*
 * Generate theme.css from NODE_THEME nodes under root.
 * First theme → :root; additional themes → .theme-{name}.
 *
 * Color-like tokens → --color-{key}; numeric tokens (e.g. radius) → --{key} with px.
 * Also emits optional utility classes (.text-primary, .bg-primary, …).
 *
 * Returns a heap string (never NULL). Caller must free().
 */
char *theme_css_generate(Node *root);

/*
 * Generate theme.css from IR only (no AST origin).
 * Walks ir->root for IR_HOOK name=="theme"; token attrs are IR_ATTR kids.
 * Same CSS shape as theme_css_generate. Never NULL; caller frees.
 */
char *theme_css_generate_from_ir(const IrProgram *ir);

/*
 * True if `val` is a Cord theme color token (primary, $muted, …)
 * rather than a Tailwind palette class (blue-600, white, gray-500).
 */
int theme_is_color_token(const char *val);

/*
 * Strip optional leading '$' from a theme token into buf (NUL-terminated).
 * Returns buf, or NULL if val is empty.
 */
const char *theme_token_name(const char *val, char *buf, size_t bufsz);

#endif
