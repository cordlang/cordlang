#ifndef CORDLANG_COMPILE_SERVICE_H
#define CORDLANG_COMPILE_SERVICE_H

/* Use case: compile .cord → target source string */
char *compile_service_file(const char *cord_path, const char *backend_name);
int compile_service_to_file(const char *cord_path, const char *backend_name,
                            const char *out_path);
/* Same as to_file, optionally write a Source Map v3 stub (out_path.map). */
int compile_service_to_file_ex(const char *cord_path, const char *backend_name,
                               const char *out_path, int write_sourcemap);
/* Compile to malloc'd string; if write_sourcemap and map_out_path, write stub. */
char *compile_service_file_ex(const char *cord_path, const char *backend_name,
                              int write_sourcemap, const char *map_out_path);

#endif
