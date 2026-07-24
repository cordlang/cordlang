#ifndef CORDLANG_CHECK_SERVICE_H
#define CORDLANG_CHECK_SERVICE_H

#include "domain/diag.h"

/* Lightweight type/semantic checks after project parse.
 * Fills *out with diagnostics (caller owns via diag_list_free).
 * Returns 0 if no errors (warnings OK), 1 if errors. */
int check_service_run(const char *entry_path, DiagList *out);

/* Same checks on an in-memory buffer (LSP didOpen/didChange).
 * file_label is used in diagnostic paths (may be a real path or "<buffer>"). */
int check_service_run_source(const char *file_label, const char *source,
                             size_t source_len, DiagList *out);

#endif
