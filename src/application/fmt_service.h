#ifndef CORDLANG_FMT_SERVICE_H
#define CORDLANG_FMT_SERVICE_H

#include <stddef.h>

/* Format .cord file(s) under path (file or directory).
 *
 * write_in_place: rewrite files on disk (default for `cordlang fmt`).
 * check_only: do not write; return 1 if any file would change (--check).
 *
 * Returns 0 on success, 1 on check failure or I/O/format error.
 */
int fmt_service_run(const char *path, int write_in_place, int check_only);

/* Normalize source text (tabs→spaces, trim, blank collapse). Caller frees. */
char *fmt_service_normalize(const char *src, size_t src_len);

#endif
