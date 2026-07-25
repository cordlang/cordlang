#ifndef CORDLANG_SOURCE_ATTR_H
#define CORDLANG_SOURCE_ATTR_H

#include <stddef.h>

/* Guess Cord source path for a codegen unit.
 * kind_dir: "pages" | "components" | "layouts" | NULL.
 * Writes e.g. "src/pages/HomePage.cord" into out (always null-terminated).
 * Prefers a path that exists on disk when possible. */
void cord_guess_source_path(const char *unit_name, const char *kind_dir,
                            char *out, size_t out_sz);

/* Build Source Map v3 JSON stub (mappings may be empty). Caller frees.
 * sources may be NULL / n_sources 0. */
char *cord_sourcemap_stub(const char *generated_file,
                          const char *const *sources, int n_sources);

/* Build Source Map v3 with VLQ mappings from cordlang source= markers
 * in generated content. Caller frees. */
char *cord_sourcemap_build(const char *generated_file, const char *content);

/* Write sourcemap next to generated_path (appends .map). 0 = ok.
 * Prefers mappings derived from generated file contents when present. */
int cord_write_sourcemap_file(const char *generated_path,
                              const char *const *sources, int n_sources);

#endif
