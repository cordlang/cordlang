#ifndef CORDLANG_HTML_ESCAPE_H
#define CORDLANG_HTML_ESCAPE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Escape &, <, >, ", ' into out. Truncates safely. Returns out. */
char *html_escape_to(char *out, size_t out_sz, const char *in);

#ifdef __cplusplus
}
#endif

#endif
