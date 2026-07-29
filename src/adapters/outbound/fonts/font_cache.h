#ifndef CORDLANG_FONT_CACHE_H
#define CORDLANG_FONT_CACHE_H

#include "domain/ast.h"
#include "domain/ir.h"
#include <stddef.h>

/*
 * Native Cord font cache: resolve theme `font` / `font-mono` / `font-display`
 * names to local WOFF2 under ~/.cordlang/cache/fonts (or %USERPROFILE% on
 * Windows). Download once via curl; serve / copy forever after.
 */

/* Heap path to ~/.cordlang/cache/fonts (created). Caller frees. */
char *font_cache_dir(void);

/* Slugify family ("IBM Plex Sans" → "ibm-plex-sans") into buf. */
void font_cache_slug(const char *family, char *buf, size_t n);

/*
 * Ensure family@weight is on disk. Returns 0 and writes absolute path into
 * out_path on success. On failure returns -1 (offline / unknown / no curl).
 */
int font_cache_ensure(const char *family, int weight, char *out_path,
                      size_t out_sz);

/*
 * Emit @font-face rules for theme font tokens. url_prefix e.g. "/fonts" or
 * "/@cord/fonts" (no trailing slash). resolve=1 downloads/caches as needed.
 * Never NULL; caller frees.
 */
char *font_faces_css_from_ir(const IrProgram *ir, const char *url_prefix,
                             int resolve);
char *font_faces_css_from_ast(Node *root, const char *url_prefix, int resolve);

/*
 * Copy resolved WOFF2 files for theme fonts into dest_dir (e.g. public/fonts).
 * Creates dest_dir. 0 = ok (including nothing to copy).
 */
int font_cache_install_from_ir(const IrProgram *ir, const char *dest_dir);
int font_cache_install_from_ast(Node *root, const char *dest_dir);

/* Absolute path for a cached file if present; else NULL (heap). */
char *font_cache_path_if_exists(const char *slug_weight_woff2);

#endif
