#ifndef CORDLANG_COMPILE_SERVICE_H
#define CORDLANG_COMPILE_SERVICE_H

#include <stddef.h>

/* Use case: compile .cord → target source string */
char *compile_service_file(const char *cord_path, const char *backend_name);

/*
 * Single-buffer compile (playground / WASM / tests): parse source in memory,
 * lower to IR, emit with backend. No multi-file `use` resolution.
 * backend_name "ir" returns ir_dump. Caller frees. NULL on failure.
 */
char *compile_service_source(const char *source, size_t source_len,
                             const char *backend_name, const char *file_label);
int compile_service_to_file(const char *cord_path, const char *backend_name,
                            const char *out_path);
/* Same as to_file, optionally write a Source Map v3 stub (out_path.map). */
int compile_service_to_file_ex(const char *cord_path, const char *backend_name,
                               const char *out_path, int write_sourcemap);
/* Compile to malloc'd string; if write_sourcemap and map_out_path, write stub. */
char *compile_service_file_ex(const char *cord_path, const char *backend_name,
                              int write_sourcemap, const char *map_out_path);

/* Compile with optional IR passes (names may be NULL / n=0). */
char *compile_service_file_with_passes(const char *cord_path,
                                       const char *backend_name,
                                       int write_sourcemap,
                                       const char *map_out_path,
                                       const char *const *passes, int n_passes);
int compile_service_to_file_with_passes(const char *cord_path,
                                        const char *backend_name,
                                        const char *out_path, int write_sourcemap,
                                        const char *const *passes, int n_passes);

#endif
