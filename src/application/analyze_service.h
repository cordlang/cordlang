#ifndef CORDLANG_APPLICATION_ANALYZE_SERVICE_H
#define CORDLANG_APPLICATION_ANALYZE_SERVICE_H

#include "domain/diag.h"

/* Deterministic heuristics (no LLM).
   quiet=0 prints score summary to stdout; quiet=1 suppresses it.
   If out_score non-NULL, stores 0–100 score.
   Returns 0 unless parse fails (then 1). Diagnostics go to DiagList. */
int analyze_service_run(const char *entry_path, DiagList *out);
int analyze_service_run_opts(const char *entry_path, DiagList *out, int quiet,
                             int *out_score);

#endif
