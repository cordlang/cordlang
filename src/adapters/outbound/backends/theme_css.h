#ifndef CORDLANG_THEME_CSS_H
#define CORDLANG_THEME_CSS_H

#include "domain/ast.h"

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
