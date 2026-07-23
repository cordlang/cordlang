#ifndef CORDLANG_APPLICATION_ANALYZE_SERVICE_H
#define CORDLANG_APPLICATION_ANALYZE_SERVICE_H

#include "domain/diag.h"

/* Deterministic heuristics (no LLM). Prints a short score summary to stdout.
   Returns 0 always unless parse fails (then 1). Warnings go to DiagList. */
int analyze_service_run(const char *entry_path, DiagList *out);

#endif
