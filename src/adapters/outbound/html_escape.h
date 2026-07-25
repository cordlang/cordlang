#ifndef CORDLANG_HTML_ESCAPE_H
#define CORDLANG_HTML_ESCAPE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Escape &, <, >, ", ' into out. Truncates safely. Returns out. */
char *html_escape_to(char *out, size_t out_sz, const char *in);

/*
 * Escape for contents of a double-quoted JS/JSON string.
 * Writes into out (truncated). Returns out.
 */
char *js_escape_dq_to(char *out, size_t out_sz, const char *in);

/* Same for single-quoted JS strings (Svelte emit). */
char *js_escape_sq_to(char *out, size_t out_sz, const char *in);

/* Heap copies (caller frees). Empty string on NULL in / OOM. */
char *js_escape_dq_dup(const char *in);
char *js_escape_sq_dup(const char *in);

/* Return 1 if href/URL is safe for emission (blocks javascript:/data:/vbscript:). */
int url_href_is_safe(const char *href);

#ifdef __cplusplus
}
#endif

#endif
